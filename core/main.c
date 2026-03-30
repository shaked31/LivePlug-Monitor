#include "../include/core_engine.h"

#define PLUGINS_DIR "./bin/plugins"

int main() {

    engine_load_existing_plugins(PLUGINS_DIR);
    engine_start_monitor(PLUGINS_DIR);
    
    return 0;
}