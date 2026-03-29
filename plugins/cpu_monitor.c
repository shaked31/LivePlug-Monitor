#include "../include/plugin_api.h"
#include <stdio.h>

#define PATH_TO_STAT_FILE "/proc/stat"

static FILE* stat_file = NULL;

void cpu_init() {
    stat_file = fopen(PATH_TO_STAT_FILE, 'r');
}