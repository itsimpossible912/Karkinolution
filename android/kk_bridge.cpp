#include "android/kk_bridge.h"

#include "karkinolution/networking/client_side/client/alias.hpp"
#include "karkinolution/networking/client_side/interpreters/system.hpp"
#include "karkinolution/networking/client_side/world/commanders/creature.hpp"
#include "karkinolution/networking/client_side/world/world.hpp"
#include "karkinolution/networking/server_side/server/server.hpp"
#include "karkinolution/world/world.hpp"
#include "karkinolution/world/world_storage.hpp"

#include <asio/io_context.hpp>
#include <karkinolution/networking/port.hpp>
#include <magic_enum/magic_enum.hpp>

#include <cstring>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace CW = Karkinolution::Networking::ClientSide::World;
namespace CC = Karkinolution::Networking::ClientSide::World::Commanders::Creature;

namespace {
struct ServerState {
        asio::io_context                   io;
        Karkinolution::World::WorldStorage storage;
        std::unique_ptr<Karkinolution::Networking::ServerSide::Server::Server> server;
        std::thread                        thread;
};

std::mutex                                  g_mutex;
std::unique_ptr<ServerState>                g_server;
std::unique_ptr<KarkinolutionRuntimeClient> g_client;
std::unique_ptr<CW::World>                  g_world;

Karkinolution::World::WorldPreset make_preset(int seed) {
        return Karkinolution::World::WorldPreset{
                .seed    = seed,
                .size    = KShared::Math::Units::Size{.width  = KShared::Math::Units::Width{100.0},
                                                                                          .height = KShared::Math::Units::Height{100.0}},
                .epsilon = 5.0f};
}

std::string esc(const std::string &s) {
        std::string r;
        for (char c : s) {
                if (c == '"' || c == '\\') r += '\\';
                r += c;
        }
        return r;
}
} // namespace

extern "C" int kk_start_server() {
        std::lock_guard lock(g_mutex);
        if (g_server) return 0;
        try {
                auto s = std::make_unique<ServerState>();
                Karkinolution::World::World world{make_preset(1)};
                s->storage.add(std::move(world));
                s->server = std::make_unique<Karkinolution::Networking::ServerSide::Server::Server>(
                        s->io, Karkinolution::Networking::PORT, s->storage);
                s->thread = std::thread([p = s.get()] { p->io.run(); });
                g_server  = std::move(s);
                return 0;
        } catch (...) {
                return 1;
        }
}

extern "C" void kk_stop_server() {
        std::lock_guard lock(g_mutex);
        if (!g_server) return;
        g_server->io.stop();
        if (g_server->thread.joinable()) g_server->thread.join();
        g_server.reset();
}

extern "C" int kk_connect() {
        std::lock_guard lock(g_mutex);
        try {
                g_client = std::make_unique<KarkinolutionRuntimeClient>();
                if (!g_client->connect()) {
                        g_client.reset();
                        return 1;
                }
                g_world = std::make_unique<CW::World>();
                if (!SystemInterpreter::create_world(*g_client, make_preset(2), true)) return 2;
                return 0;
        } catch (...) {
                return 3;
        }
}

extern "C" void kk_disconnect() {
        std::lock_guard lock(g_mutex);
        if (g_client) g_client->disconnect();
        g_client.reset();
        g_world.reset();
}

extern "C" int kk_create_creature(const char *species, double x, double y) {
        std::lock_guard lock(g_mutex);
        if (!g_client || !g_world) return 1;
        try {
                std::optional<CW::CreatureSpecies> sp;
                if (species && *species) {
                        sp = magic_enum::enum_cast<CW::CreatureSpecies>(std::string_view(species));
                        if (!sp) return 4;
                }
                return CC::make_creature(*g_client, *g_world, std::nullopt, sp,
                                         std::optional<KShared::Math::Vec::Vec2>(KShared::Math::Vec::Vec2(x, y)),
                                         std::nullopt) ? 0 : 2;
        } catch (...) {
                return 3;
        }
}

extern "C" int kk_tick_all() {
        std::lock_guard lock(g_mutex);
        if (!g_client || !g_world) return -1;
        try {
                std::vector<CW::OrganismId> ids;
                for (const auto &c : g_world->get_creatures()) ids.push_back(c.id);
                int ok = 0;
                for (auto id : ids) {
                        if (CC::tick_creature(*g_client, *g_world, id)) ++ok;
                }
                return ok;
        } catch (...) {
                return -2;
        }
}

extern "C" int kk_snapshot(char *buf, int cap) {
        std::lock_guard lock(g_mutex);
        std::ostringstream o;
        o << "{\"world\":{\"w\":100,\"h\":100},\"creatures\":[";
        if (g_world) {
                bool first = true;
                for (const auto &c : g_world->get_creatures()) {
                        if (!first) o << ",";
                        first = false;
                        o << "{\"id\":\"" << static_cast<unsigned long long>(c.id) << "\""
                          << ",\"name\":\"" << esc(c.name) << "\""
                          << ",\"species\":\"" << magic_enum::enum_name(c.specie) << "\""
                          << ",\"gender\":\"" << magic_enum::enum_name(c.gender) << "\""
                          << ",\"x\":" << c.position.x << ",\"y\":" << c.position.y
                          << ",\"age\":" << c.age
                          << ",\"life\":" << c.vital.life.value << ",\"lifeMax\":" << c.vital.life.max
                          << ",\"health\":" << c.vital.health.value << ",\"healthMax\":" << c.vital.health.max
                          << ",\"energy\":" << c.metabolism.energy.value << ",\"energyMax\":" << c.metabolism.energy.max
                          << "}";
                }
        }
        o << "]}";
        const std::string s = o.str();
        if (buf && cap > static_cast<int>(s.size())) std::memcpy(buf, s.c_str(), s.size() + 1);
        return static_cast<int>(s.size());
}
