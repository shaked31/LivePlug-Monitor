#define _XOPEN_SOURCE 500
#include "../include/plugin_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#define PATH_TO_MEM_FILE "/proc/meminfo"
#define MAX_BUFFER_SZ 2048
static int mem_fd;

int mem_init() {
    mem_fd = open(PATH_TO_MEM_FILE, O_RDONLY);
    if (mem_fd < 0) {
        fprintf(stderr, "Couldn't open /proc/meminfo\n");
        return EXIT_FAILURE;
    }
    printf("CPU Monitor Plugin initialized\n");
    return EXIT_SUCCESS;
}

void mem_run() {
    if (mem_fd < 0)
        return;
    
    char buffer[MAX_BUFFER_SZ];
    ssize_t bytes_read = pread(mem_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read <= 0)
        return;
    buffer[bytes_read] = '\0';


    unsigned long total = 0, available = 0;
    char *ptr_total = strstr(buffer, "MemTotal:");
    char *ptr_available = strstr(buffer, "MemAvailable:");

    if (ptr_total)
        sscanf(ptr_total, "MemTotal: %lu", &total);
    if (ptr_available)
        sscanf(ptr_available, "MemAvailable: %lu", &available);

    if (total > 0) {
        float used_gb = (float)(total - available) / (1024 * 1024);
        float total_gb = (float)total / (1024 * 1024);
        printf("[MEM] Usage: %.2f GB / %.2f GB (%.1f%%)\n", 
               used_gb, total_gb, (float)(total - available) / total * 100.0);
    }
}

void mem_cleanup() {
    if (mem_fd >= 0)
        close(mem_fd);
}

plugin_t* get_plugin() {
    static plugin_t p = {
        .name = "mem_plugin",
        .init = mem_init,
        .run = mem_run,
        .cleanup = mem_cleanup
    };
    return &p;
}