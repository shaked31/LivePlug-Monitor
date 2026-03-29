#ifndef CORE_ENGINE_H
#define CORE_ENGINE_H

#include "plugin_api.h"

int engine_load_plugin(const char* path, const char* filename); // loads plugin & adds to the linked list
void engine_cleanup_plugin(const char* filename);
// static void engine_add_plugin_to_list(const char* name, plugin_t* p, void* handle);
void engine_load_existing_plugins(const char* path);
void engine_start_monitor(const char* path);

#endif