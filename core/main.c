#include "../include/core_engine.h"
// #include "../include/plugin_api.h"
// #include "../include/utils.h"

// #include <stdio.h>
// #include <unistd.h>
// #include <string.h>
// #include <sys/inotify.h>

#define PLUGINS_DIR "./bin/plugins"

int main() {

    engine_load_existing_plugins(PLUGINS_DIR);
    engine_start_monitor(PLUGINS_DIR);
    
    return 0;
}