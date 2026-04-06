#include "../include/ui_manager.h"
#include "../include/common_defs.h"

#include <pthread.h>
#include <string.h>
#include <stdlib.h>

#define MIN_TERMINAL_WIDTH 90

#define UI_LOG_HEIGHT 8
#define UI_PLUGIN_LOG_HEIGHT 8
#define UI_HEADER_HEIGHT 2
#define UI_FOOTER_HEIGHT 1
#define UI_SPACING 2

#define MIN_TERMINAL_HEIGHT (UI_LOG_HEIGHT + UI_PLUGIN_LOG_HEIGHT + UI_HEADER_HEIGHT + UI_FOOTER_HEIGHT + UI_SPACING + MAX_PLUGINS)


static pthread_mutex_t ui_mutex = PTHREAD_MUTEX_INITIALIZER;
static WINDOW *log_win = NULL, *plugin_log_win = NULL, *monitor_win = NULL;

void ui_init() {
    initscr();
    cbreak();
    noecho();
    curs_set(0);

    int max_x, max_y;
    getmaxyx(stdscr, max_y, max_x);
    if (max_x < MIN_TERMINAL_WIDTH || max_y < MIN_TERMINAL_HEIGHT) {
        endwin();
        printf("[SYS Error]: Terminal is too small (%dx%d)\n", max_x, max_y);
        printf("Resize your terminal to at least (%dx%d) and try again\n", MIN_TERMINAL_WIDTH, MIN_TERMINAL_HEIGHT);
        exit(EXIT_FAILURE);
    }

    log_win = newwin(UI_LOG_HEIGHT, max_x, 0, 0);
    scrollok(log_win, TRUE);

    int plugin_start_y = UI_LOG_HEIGHT + 1;
    plugin_log_win = newwin(UI_PLUGIN_LOG_HEIGHT, max_x, plugin_start_y, 0);
    scrollok(plugin_log_win, TRUE);

    int mon_start_y = plugin_start_y + UI_PLUGIN_LOG_HEIGHT + 1;
    monitor_win = newwin(max_y - mon_start_y, max_x, mon_start_y, 0);

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

void safe_print(WINDOW* win, int row, const char* fmt, ...) {
    if (win == NULL)
        return;
    
    pthread_mutex_lock(&ui_mutex);
    int mon_attrs = A_BOLD | A_REVERSE;

    bool is_monitor = (win == monitor_win);
    if (is_monitor && row >= 0) {
        wattron(win, mon_attrs);
        wmove(win, row, 0);
        wclrtoeol(win);
    }
    
    va_list args;
    va_start(args, fmt);
    vw_printw(win, fmt, args);
    va_end(args);

    if (is_monitor && row >= 0) {
        wattroff(win, mon_attrs);
        if (strstr(fmt, "-----") != NULL) {
            wclrtobot(win); // clears the screen
        }
    }
        

    wrefresh(win);
    pthread_mutex_unlock(&ui_mutex);
}