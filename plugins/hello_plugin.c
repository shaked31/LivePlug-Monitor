#include "../include/plugin_api.h"
#include "../include/ui_manager.h"

int hello_init(WINDOW *plugin_log_win) {
    safe_print(plugin_log_win, -1, "[HELLO Info] hello there, Im an hello plugin\n");
    return 0;
}

void hello_run(WINDOW *mon_win, WINDOW *plugin_log_win, int monitor_row_idx) {
    (void)plugin_log_win;
    safe_print(mon_win, monitor_row_idx, "[HELLO] running hello plugin...\n");
}

void hello_cleanup(WINDOW *plugin_log_win) {
    safe_print(plugin_log_win, -1, "[HELLO Info] cleaning up hello plugin... BYE!\n");
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