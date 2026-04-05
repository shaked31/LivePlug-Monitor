#include "../include/plugin_api.h"
#include <stdio.h>

int hello_init(WINDOW *plugin_log_win) {
    wprintw(plugin_log_win, "[HELLO] hello there, Im an hello plugin\n");
    wrefresh(plugin_log_win);
    return 0;
}

void hello_run(WINDOW *mon_win, WINDOW *plugin_log_win) {
    (void)plugin_log_win;
    wprintw(mon_win, "[HELLO] running hello plugin...\n");
    wrefresh(mon_win);
}

void hello_cleanup(WINDOW *plugin_log_win) {
    wprintw(plugin_log_win, "[HELLO] cleaning up hello plugin... BYE!\n");
    wrefresh(plugin_log_win);
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