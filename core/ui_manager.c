#include "../include/ui_manager.h"
#include <stdarg.h>
#include <stdlib.h>

static WINDOW *log_win = NULL, *plugin_log_win = NULL, *monitor_win = NULL;

void ui_init() {
    initscr();
    cbreak();
    noecho();
    curs_set(0);

    int max_x, max_y;
    getmaxyx(stdscr, max_y, max_x);

    int log_h = 5;
    int plugin_log_h = 6;
    int mon_log_h = max_y - log_h - plugin_log_h;

    if (mon_log_h < 10) {
        endwin();
        fprintf(stderr, "[SYS Error]: Terminal too small. (Current height: %d)\n", max_y);
        exit(EXIT_FAILURE);
    }

    log_win = newwin(log_h, max_x, 0, 0);
    scrollok(log_win, TRUE);

    plugin_log_win = newwin(plugin_log_h, max_x, log_h + 1, 0);
    scrollok(plugin_log_win, TRUE);

    monitor_win = newwin(mon_log_h, max_x, log_h + plugin_log_h + 2, 0);
    // scrollok(plugin_log_win, TRUE);

    wprintw(log_win, "[SYS Info]: ncurses UI initialized\n");
    wrefresh(log_win);
}

void ui_log(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vw_printw(log_win, fmt, args);
    va_end(args);
    wrefresh(log_win);
}

WINDOW* ui_get_monitor_win() {
    return monitor_win;
}

WINDOW* ui_get_plugin_log_win() {
    return plugin_log_win;
}

void ui_refresh_monitor() {
    wrefresh(monitor_win);
}

void ui_clear_monitor() {
    wclear(monitor_win);
}

void ui_cleanup() {
    endwin();
}