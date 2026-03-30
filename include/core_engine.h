#ifndef CORE_ENGINE_H
#define CORE_ENGINE_H

#include "plugin_api.h"

// --------- static functions --------- //
int engine_load_plugin(const char* path); // loads plugin & adds to the linked list
void engine_cleanup_plugin(const char* filename);
void engine_run_all();
void handle_sigint(int sig);
void restore_terminal();

// --------- non-static functions --------- //
void engine_load_existing_plugins(const char* path);
void engine_start_monitor(const char* path);

#endif