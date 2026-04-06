/**
 * thermal_monitor.c: A dynamic plugin for monitoring CPU Temperature in real-time.
 * This plugin performs dynamic discovery of thermal sensors in /sys/class/thermal dir
 * and binds to the x86_pkg_temp (CPU Package) sensor if available.
 */

#include "../include/plugin_api.h"
#include "../include/ui_manager.h"

#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>

#define MAX_THERMAL_PATH 512
#define MAX_THERMAL_TYPE_PATH 512
#define MAX_THERMAL_TYPE 32
#define MAX_THERMAL_NAME 64
#define MAX_THERMAL_TEMP_LEN 16
#define PATH_TO_THERMAL_DIR "/sys/class/thermal"
#define DEFAULT_NAME_IN_THERMAL_DIR "thermal_zone"
#define DEFAULT_CPU_THERMAL_TYPE "pkg_temp"

static char thermal_path[MAX_THERMAL_PATH];
static int thermal_fd;

/**
 * Initializes the thermal monitor plugin
 * Searches for available sensors
 * Opens the temperature file in the sensors folder for the run function
 */
int thermal_init(WINDOW *plugin_log_win) {
    struct dirent *entry;
    DIR *dir = opendir(PATH_TO_THERMAL_DIR);
    bool found = false;

    if (dir == NULL) {
        safe_print(plugin_log_win, -1, "[THERMAL Error] Couldn't open dir '%s'\n", PATH_TO_THERMAL_DIR);
        return EXIT_FAILURE;
    }
    char thermal_sensor[MAX_THERMAL_NAME];

    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, DEFAULT_NAME_IN_THERMAL_DIR) != NULL) {
            char thermal_type_path[MAX_THERMAL_TYPE_PATH];
            char thermal_type[MAX_THERMAL_TYPE];
            snprintf(thermal_type_path, sizeof(thermal_type_path), "/sys/class/thermal/%s/type", entry->d_name);

            int fd = open(thermal_type_path, O_RDONLY);
            if (fd < 0) {
                safe_print(plugin_log_win, -1, "[THERMAL Error] Couldn't open file '%s'\n",  thermal_type_path);
                continue;
            }

            ssize_t bytes_read = read(fd, thermal_type, sizeof(thermal_type) - 1);
            close(fd);

            if (bytes_read <= 0)
                return EXIT_FAILURE;
            thermal_type[bytes_read] = '\0';
            thermal_type[strcspn(thermal_type, "\r\n ")] = 0;

            if (strstr(thermal_type, DEFAULT_CPU_THERMAL_TYPE) != NULL) {
                snprintf(thermal_path, sizeof(thermal_path), "/sys/class/thermal/%s/temp", entry->d_name);
                found = true;
                strncpy(thermal_sensor, entry->d_name, sizeof(thermal_sensor));
                thermal_fd = open(thermal_path, O_RDONLY);
                if (thermal_fd < 0) {
                    safe_print(plugin_log_win, -1, "[THERMAL Error] Couldn't open file '%s'\n", thermal_path);
                    return EXIT_FAILURE;
                }
                break;
            }
        }
    }
    closedir(dir);

    if (!found) {
        safe_print(plugin_log_win, -1, "[THERMAL Warning] No thermal sensors found on this system\n");
        return EXIT_FAILURE;
    }
    
    safe_print(plugin_log_win, -1, "[THERMAL Info] Bound to sensor %s\n", thermal_sensor);
    return EXIT_SUCCESS;
}

/**
 * The main execution login for the plugin
 * Reads the file that was opened by the init function
 * Prints the temperature to the monitor window
 */
void thermal_run(WINDOW *mon_win, WINDOW *plugin_log_win, int monitor_row_idx) {
    if (thermal_fd < 0)
        return;

    int temp_raw;
    char buffer[MAX_THERMAL_TEMP_LEN];
    ssize_t bytes_read = pread(thermal_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read <= 0)
        return;
    buffer[bytes_read] = '\0';
    if (sscanf(buffer, "%d", &temp_raw) == 1) {
        safe_print(mon_win, monitor_row_idx, "[THERMAL] CPU Temperature is %.1fC", temp_raw / 1000.0);
    }
    else {
        safe_print(mon_win, monitor_row_idx, "[THERMAL] CPU Temperature is N/A");
        safe_print(plugin_log_win, -1, "[THERMAL Error] Failed to parse temperature value\n");
    }
}

/**
 * Closes file descriptor
 */
void thermal_cleanup(WINDOW *plugin_log_win) {
    if (thermal_fd >= 0)
        close(thermal_fd);
    safe_print(plugin_log_win, -1, "[THERMAL Info] Finished cleaning up\n");
}

/**
 * Export the plugin interface for the core engine
 */
plugin_t* get_plugin() {
    static plugin_t p = {
        .name = "thermal_plugin",
        .init = thermal_init,
        .run = thermal_run,
        .cleanup = thermal_cleanup
    };
    return &p;
}