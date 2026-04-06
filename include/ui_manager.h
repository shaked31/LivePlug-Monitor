#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <ncurses.h>
#include <stdarg.h>

void ui_init();

void ui_log(const char* fmt, ...);

WINDOW* ui_get_monitor_win();
WINDOW* ui_get_plugin_log_win();

void ui_clear_monitor();
void ui_refresh_monitor();
void ui_cleanup();

void safe_print(WINDOW* win, int row, const char* fmt, ...);

#endif