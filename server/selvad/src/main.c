#include "cdefs.h"
#include "event_loop.h"
#include "module.h"

int main(void)
{
    evl_init();

    evl_load_module("modules/libsignal.so");
    evl_load_module("modules/libdemo_timeout.so");
    evl_load_module("modules/libdemo_async.so");
    evl_load_module("modules/libdemo_sock.so");

    evl_start();
    evl_deinit();

    return 0;
}
