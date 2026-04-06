/**
 * cpu_monitor.c: A dynamic plugin for monitoring CPU usage
 * This plugin reads statistics from Linux's /proc/stat file
 * It calculates the precentage of CPU usage by delta (between 2 time periods)
 */

#define _XOPEN_SOURCE 500 // Required for pread()

#include "../include/plugin_api.h"
#include "../include/ui_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>

#define PATH_TO_CPU_FILE "/proc/stat"
#define MAX_BUFFER_SZ 1024
static int cpu_fd;
static long long last_total = 0, last_idle = 0;
static bool first_run = true;

/**
 * Initializes the CPU monitor
 * Opens the file, kept open for performance
 * Initializes relevant static variables
 */
int cpu_init(WINDOW *plugin_log_win) {
    cpu_fd = open(PATH_TO_CPU_FILE, O_RDONLY);
    if (cpu_fd < 0) {
        safe_print(plugin_log_win, -1, "[CPU Error] Couldn't open /proc/stat\n");
        return EXIT_FAILURE;
    }
    safe_print(plugin_log_win, -1, "[CPU Info] CPU Monitor Plugin initialized\n");
    first_run = true;
    last_total = 0;
    last_idle = 0;

    return EXIT_SUCCESS;
}

/**
 * The main execution login for the plugin
 * Calculates CPU usage percentage based on the formula:
 * Usage = 100 * (TotalDiff - IdleDiff) / TotalDiff
 */
void cpu_run(WINDOW *mon_win, WINDOW *plugin_log_win, int monitor_row_idx) {
    (void)plugin_log_win;
    if (cpu_fd < 0)
        return;

    char buffer[MAX_BUFFER_SZ];

    /**
     * Using pread() with offset 0 to perform an atomic "seek and read" (the CPU treats it as a single action)
     * This avoids calling lseek to reset the file cursor manually before every sample
     * Reducing syscalls and CPU overhead
     */ 

    ssize_t bytes_read = pread(cpu_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read <= 0)
        return;
    buffer[bytes_read] = '\0';

    /** This are the variables that appear in the first line of the /proc/stat file
     * user:    Time spent in user mode
     * nice:    Time spent in user mode with low priority
     * system:  Time spent in kernel mode
     * idle:    Time spent doing nothing
     * iowait:  Time spent waiting for I/O to complete
     * irq:     Time spent servicing hardware interrupts
     * softirq: Time spent servicing software interrupts
     * steal:   Time spent in other OSes when running in a virtualized environment
     */
    long long user, nice, system, idle, iowait, irq, softirq, steal;
    
    // fscanf returns the number of variables it managed to read and write into successfully. 4 - the basic variables we need user,nice,system,idle
    if (sscanf(buffer, "cpu %lld %lld %lld %lld %lld %lld %lld %lld",
                &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal) < 4)
                return;
    
    // Seperate CPU time into Idle and Non-Idle
    long long curr_idle = idle + iowait;
    long long curr_non_idle = user + nice + system + irq + softirq + steal;
    long long curr_total = curr_idle + curr_non_idle;

    /**
     * CPU usage is only meaningfull as a change over time (we need to calculate the delta)
     * On the first run, we just capture the first measurement
     */
    if (!first_run) {
        long long total_diff = curr_total - last_total;
        long long idle_diff = curr_idle - last_idle;

        if (total_diff > 0) {
            // Calculate percentage of time not spent in Idle state
            float usage = (float)(total_diff - idle_diff) / total_diff * 100.0;
            
            if (usage > 100)
                usage = 100;
            else if (usage < 0)
                usage = 0;

            safe_print(mon_win, monitor_row_idx, "[CPU] Usage: %.2f%%\n", usage);
        }
    }
    else {
        safe_print(mon_win, monitor_row_idx, "[CPU] Calculating...\n");
        first_run = false;
    }
    
    last_total = curr_total;
    last_idle = curr_idle;
}

/**
 * Closes file descriptor
 */
void cpu_cleanup(WINDOW *plugin_log_win) {
    if (cpu_fd >= 0)
        close(cpu_fd);
    safe_print(plugin_log_win, -1, "[CPU Info] Finished cleaning up\n");
}

/**
 * Export the plugin interface for the core engine
 */
plugin_t* get_plugin() {
    static plugin_t p = {
        .name = "cpu_plugin",
        .init = cpu_init,
        .run = cpu_run,
        .cleanup = cpu_cleanup
    };
    return &p;
}