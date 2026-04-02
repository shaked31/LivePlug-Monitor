#include "../include/ui_manager.h"
#include <stdarg.h>

static WINDOW *log_win = NULL, *plugin_log_win = NULL, *monitor_win = NULL;

void ui_init() {
    initscr();
    cbreak();
    noecho();
    curs_set(0);

    int max_x, max_y;
    getmaxyx(stdscr, max_y, max_x);

    log_win = newwin(max_y / 3, max_x, 0, 0);
    scrollok(log_win, TRUE);

    plugin_log_win = newwin(max_y / 3, max_x, max_y / 3, 0);
    scrollok(plugin_log_win, TRUE);

    monitor_win = newwin(max_y / 3, max_x, 2 * (max_y / 3), 0);

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