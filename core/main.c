#include "../include/core_engine.h"

#define PLUGINS_DIR "./bin/plugins"

/**
 * Entry point for the LivePlug Monitor application.
 * Loads existing plugins and starts the inotify-based monitoring loop.
 * Handles Ctrl+C gracefully via signal handler.
 */
int main() {

    engine_load_existing_plugins(PLUGINS_DIR);
    engine_start_monitor(PLUGINS_DIR);
    
    return 0;
}