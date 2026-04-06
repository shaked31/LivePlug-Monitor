#include "../include/ui_manager.h"
#include "../include/common_defs.h"

#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#define MIN_TERMINAL_WIDTH 90

#define UI_LOG_HEIGHT 8
#define UI_PLUGIN_LOG_HEIGHT 8
#define UI_HEADER_HEIGHT 2
#define UI_FOOTER_HEIGHT 1
#define UI_SPACING 2

#define MIN_TERMINAL_HEIGHT (UI_LOG_HEIGHT + UI_PLUGIN_LOG_HEIGHT + UI_HEADER_HEIGHT + UI_FOOTER_HEIGHT + UI_SPACING + MAX_PLUGINS)


static pthread_mutex_t ui_mutex = PTHREAD_MUTEX_INITIALIZER;
static WINDOW *log_win = NULL, *plugin_log_win = NULL, *monitor_win = NULL;
static bool is_ui_active = false;

/**
 * Initializes ncurses UI with three windows:
 * - log_win: system log output
 * - plugin_log_win: plugin initialization and debug output
 * - monitor_win: live dashboard with plugin status
 * Validates terminal size and exits if too small.
 */
void ui_init() {
    initscr();
    cbreak();
    noecho();
    curs_set(0);

    int max_x, max_y;
    getmaxyx(stdscr, max_y, max_x);
    if (max_x < MIN_TERMINAL_WIDTH || max_y < MIN_TERMINAL_HEIGHT) {
        endwin();
        printf("[UI Error]: Terminal is too small (%dx%d)\n", max_x, max_y);
        printf("Resize your terminal to at least (%dx%d) and try again\n", MIN_TERMINAL_WIDTH, MIN_TERMINAL_HEIGHT);
        exit(EXIT_FAILURE);
    }
    is_ui_active = true;

    log_win = newwin(UI_LOG_HEIGHT, max_x, 0, 0);
    scrollok(log_win, TRUE);

    int plugin_start_y = UI_LOG_HEIGHT + 1;
    plugin_log_win = newwin(UI_PLUGIN_LOG_HEIGHT, max_x, plugin_start_y, 0);
    scrollok(plugin_log_win, TRUE);

    int mon_start_y = plugin_start_y + UI_PLUGIN_LOG_HEIGHT + 1;
    monitor_win = newwin(max_y - mon_start_y, max_x, mon_start_y, 0);

    wprintw(log_win, "[UI Info]: ncurses UI initialized\n");
    wrefresh(log_win);
}

/**
 * Thread-safe logging function using variadic arguments.
 * Writes to ncurses log window if UI is initialized, otherwise prints to stdout.
 * Automatically flushes output after each write.
 */
void ui_log(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    if (is_ui_active && log_win != NULL) {
        vw_printw(log_win, fmt, args);
        wrefresh(log_win);
    }
    else {
        vprintf(fmt, args);
        fflush(stdout);
    }
    va_end(args);
    
}

/**
 * Returns the monitor window for displaying plugin status output.
 */
WINDOW* ui_get_monitor_win() {
    return monitor_win;
}

/**
 * Returns the plugin log window for plugin initialization and debug output.
 */
WINDOW* ui_get_plugin_log_win() {
    return plugin_log_win;
}

/**
 * Refreshes the monitor window display by calling wrefresh().
 */
void ui_refresh_monitor() {
    wrefresh(monitor_win);
}

/**
 * Clears all content from the monitor window.
 */
void ui_clear_monitor() {
    wclear(monitor_win);
}

/**
 * Cleans up ncurses UI and restores terminal to normal mode.
 * Restores terminal flags (echo, canonical mode) and cursor visibility.
 * Clears screen and resets terminal state before exit.
 */
void ui_cleanup() {
    if (!is_ui_active)
        return;
    
    is_ui_active = false;
    erase();
    refresh();
    endwin();
    // struct termios term;
    // if (tcgetattr(STDOUT_FILENO, &term) == 0) {
    //     term.c_lflag |= (ECHO | ICANON);  // restore input echo and canonical mode
    //     term.c_oflag |= (OPOST | ONLCR);  // restore output processing and line feed conversion
    //     tcsetattr(STDOUT_FILENO, TCSANOW, &term);
    // }

    fflush(stdout);
}

void safe_print(WINDOW* win, int row, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    if (win == NULL || !is_ui_active) {
        vprintf(fmt, args);
        va_end(args);
        fflush(stdout);
        return;
    }
        
    
    pthread_mutex_lock(&ui_mutex);
    int mon_attrs = A_BOLD | A_REVERSE;

    bool is_monitor = (win == monitor_win);
    if (is_monitor && row >= 0) {
        wattron(win, mon_attrs);
        wmove(win, row, 0);
        wclrtoeol(win);
    }
    

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