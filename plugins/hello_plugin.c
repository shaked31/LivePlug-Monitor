#include "../include/plugin_api.h"
#include <stdio.h>

int hello_init() {
    printf("hello there, Im an hello plugin\n");
    return 0;
}

void hello_run() {
    printf("running hello plugin...\n");
}

void hello_cleanup() {
    printf("cleaning up hello plugin... BYE!\n");
}

plugin_t* get_plugin() {
    static plugin_t p = {
        .name = "hello_plugin",
        .init = hello_init,
        .run = hello_run,
        .cleanup = hello_cleanup
    };
    return &p;
}