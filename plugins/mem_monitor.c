#define _XOPEN_SOURCE 500
#include "../include/plugin_api.h"
#include "../include/ui_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#define PATH_TO_MEM_FILE "/proc/meminfo"
#define MAX_BUFFER_SZ 2048
static int mem_fd;

/**
 * Initializes the Memory usage monitor
 * Opens the file, kept open for performance
 */
int mem_init(WINDOW *plugin_log_win) {
    mem_fd = open(PATH_TO_MEM_FILE, O_RDONLY);
    if (mem_fd < 0) {
        safe_print(plugin_log_win, -1, "[MEM Error] Couldn't open %s\n", PATH_TO_MEM_FILE);
        return EXIT_FAILURE;
    }
    safe_print(plugin_log_win, -1, "[MEM Info] Memory Monitor Plugin initialized\n");
    return EXIT_SUCCESS;
}

void mem_run(WINDOW *mon_win, WINDOW *plugin_log_win, int monitor_row_idx) {
    (void)plugin_log_win;
    if (mem_fd < 0)
        return;
    
    char buffer[MAX_BUFFER_SZ];

    /**
     * Using pread() with offset 0 to perform an atomic "seek and read" (the CPU treats it as a single action)
     * This avoids calling lseek to reset the file cursor manually before every sample
     * Reducing syscalls and CPU overhead
     */
    ssize_t bytes_read = pread(mem_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read <= 0)
        return;
    buffer[bytes_read] = '\0';


    unsigned long total = 0, available = 0;

    
    /**
     * Search for specific keys in the /proc/meminfo file
     * MemTotal - Total physical RAM installed
     * MemAvailable - Estimated memory available
     */ 
    char *ptr_total = strstr(buffer, "MemTotal:");
    char *ptr_available = strstr(buffer, "MemAvailable:");

    if (ptr_total)
        sscanf(ptr_total, "MemTotal: %lu", &total);
    if (ptr_available)
        sscanf(ptr_available, "MemAvailable: %lu", &available);

    // Calculates values in GB
    if (total > 0) {
        float used_gb = (float)(total - available) / (1024 * 1024);
        float total_gb = (float)total / (1024 * 1024);
        safe_print(mon_win, monitor_row_idx, "[MEM] Usage: %.2f GB / %.2f GB (%.1f%%)\n", 
               used_gb, total_gb, (float)(total - available) / total * 100.0);
    }
}

/**
 * Cleans up open resources
 * closes file descriptor
 */
void mem_cleanup(WINDOW *plugin_log_win) {
    if (mem_fd >= 0)
        close(mem_fd);
    safe_print(plugin_log_win, -1, "[MEM Info] Finished cleaning up\n");
}

/**
 * Export the plugin interface for the core engine
 */
plugin_t* get_plugin() {
    static plugin_t p = {
        .name = "mem_plugin",
        .init = mem_init,
        .run = mem_run,
        .cleanup = mem_cleanup
    };
    return &p;
}