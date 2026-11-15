#pragma once
extern "C" {
int  kk_start_server();
void kk_stop_server();
int  kk_connect();
void kk_disconnect();
int  kk_create_creature(const char *species, double x, double y);
int  kk_tick_all();
int  kk_snapshot(char *buf, int cap);
}
