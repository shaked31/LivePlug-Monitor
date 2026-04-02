#ifndef PLUGIN_API_H
#define PLUGIN_API_H

#include <ncurses.h>

typedef struct {
    const char* name;
    int (*init)(WINDOW *plugin_log_win);
    void (*run)(WINDOW *mon_win, WINDOW *plugin_log_win);
    void (*cleanup)(WINDOW *plugin_log_win);
} plugin_t;

typedef plugin_t* (*get_plugin_f)();

#endif