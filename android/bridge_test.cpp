#include "android/kk_bridge.h"

#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

int main() {
        std::printf("servidor: %d\n", kk_start_server());
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        std::printf("conectar: %d\n", kk_connect());
        for (int i = 0; i < 3; ++i) std::printf("criar: %d\n", kk_create_creature());
        std::vector<char> buf(65536);
        int n = kk_snapshot(buf.data(), (int)buf.size());
        std::printf("estado (%d bytes): %s\n", n, buf.data());
        std::printf("ticks ok: %d\n", kk_tick_all());
        n = kk_snapshot(buf.data(), (int)buf.size());
        std::printf("depois (%d bytes): %s\n", n, buf.data());
        kk_disconnect();
        kk_stop_server();
        return 0;
}
