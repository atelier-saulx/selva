#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <dlfcn.h>
#include "event_loop.h"
#include "module.h"

static void my_hello()
{
    printf("Hello world from a module\n");
}

__constructor void init(void)
{
    printf("Init demo_timeout\n");

    evl_import(evl_set_timeout, NULL);
    evl_import_main(evl_clear_timeout);
    // or evl_import_event_loop();

    /*
     * Random timeout.
     */
    struct timespec t1 = {
        .tv_sec = 1,
        .tv_nsec = 0,
    };
    (void)evl_set_timeout(&t1, my_hello, NULL);

    /*
     * Cancelling timeout.
     */
    struct timespec t2 = {
        .tv_sec = 5,
        .tv_nsec = 0,
    };
    int tim = evl_set_timeout(&t2, my_hello, NULL);
    evl_clear_timeout(tim);
}
