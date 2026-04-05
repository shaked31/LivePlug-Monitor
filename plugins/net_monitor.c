/*
 * net_monitor.c: Monitors network throughput (RX/TX) in real-time.
 */

#define _XOPEN_SOURCE 500

#include "../include/plugin_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>

#define PATH_TO_NET_FILE "/proc/net/dev"
#define PATH_TO_INTF_DIR "/sys/class/net"
#define LOOPBACK_INTF "lo"
#define MAX_BUFFER_SZ 2048
#define MAX_INTERFACE_NAME_LEN 16
#define MAX_STATE_BUFFER_SZ 16
#define MAX_OPERSTATE_PATH 128

static int net_fd;
static bool first_run = true;

typedef struct {
    char name[MAX_INTERFACE_NAME_LEN];
    char last_state[MAX_STATE_BUFFER_SZ];
    unsigned long long last_rx;
    unsigned long long last_tx;
    bool is_up;
} intf_data_t;

static intf_data_t* interfaces = NULL;
static int intf_counter = 0; 

static bool is_intf_up(char* intf_name, char* out_state) { // pass to out_state NULL if no need in state
    char path_to_operstate_file[MAX_OPERSTATE_PATH];
    snprintf(path_to_operstate_file, sizeof(path_to_operstate_file), "/sys/class/net/%s/operstate", intf_name);
    int intf_status_fd = open(path_to_operstate_file, O_RDONLY);

    if (intf_status_fd < 0)
        return false;

    char state[MAX_STATE_BUFFER_SZ];
        
    ssize_t bytes_read = read(intf_status_fd, state, sizeof(state) - 1);
    close(intf_status_fd);

    if (bytes_read <= 0)
        return false;

    state[bytes_read] = '\0';
    state[strcspn(state, "\r\n ")] = 0;
    
    if (out_state != NULL) {
        strncpy(out_state, state, MAX_STATE_BUFFER_SZ);
    }

    return (strcmp(state, "up") == 0);
}

static void refresh_interfaces(WINDOW *win) {
    struct dirent *entry;
    DIR *dir = opendir(PATH_TO_INTF_DIR);

    if (dir == NULL) {
        wprintw(win, "[SYS Error]: Couldn't open dir '%s'\n", PATH_TO_INTF_DIR);
        wrefresh(win);
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        bool is_loopback_intf = (strcmp(entry->d_name, LOOPBACK_INTF) == 0);
        if (entry->d_name[0] != '.' && !is_loopback_intf) {
            bool exists = false;
            for (int i = 0 ; i < intf_counter ; i++) {
                if (strcmp(interfaces[i].name, entry->d_name) == 0) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                intf_data_t* temp = realloc(interfaces, (intf_counter + 1) * sizeof(intf_data_t));
                if (temp == NULL) {
                    closedir(dir);
                    wprintw(win, "[SYS Error]: Couldn't allocate memory for interfaces array\n");
                    wrefresh(win);
                    return;
                }
                interfaces = temp;

                // set every byte of the struct to 0, and set name, is_up, last_state (last_rx, last_tx initialized with 0)
                memset(&interfaces[intf_counter], 0, sizeof(intf_data_t));
                strncpy(interfaces[intf_counter].name, entry->d_name, MAX_INTERFACE_NAME_LEN);
                interfaces[intf_counter].is_up = is_intf_up(interfaces[intf_counter].name, interfaces[intf_counter].last_state);

                wprintw(win, "[NET Info]: New interface found %s in status %s\n", interfaces[intf_counter].name, interfaces[intf_counter].last_state);
                wrefresh(win);

                intf_counter++;
            }
        }
    }
    closedir(dir);
}

/**
 * Initializes the network monitor
 * Opens the file, kept open for performance
 */
int net_init(WINDOW *plugin_log_win) {
    net_fd = open(PATH_TO_NET_FILE, O_RDONLY);
    if (net_fd < 0) {
        wprintw(plugin_log_win, "[SYS Error]: Couldn't open /proc/net/dev\n");
        wrefresh(plugin_log_win);
        return EXIT_FAILURE;
    }

    refresh_interfaces(plugin_log_win);

    if (intf_counter == 0) {
        wprintw(plugin_log_win, "[NET Error]: No physical network interfaces found\n");
        wrefresh(plugin_log_win);
        return EXIT_FAILURE;
    }

    wprintw(plugin_log_win, "[NET Info]: Network Monitor Plugin initialized\n");
    wrefresh(plugin_log_win);
    
    first_run = true;
    return EXIT_SUCCESS;
}

void net_run(WINDOW *mon_win, WINDOW *plugin_log_win) {
    if (net_fd < 0)
        return;
    
    refresh_interfaces(plugin_log_win);
    char buffer[MAX_BUFFER_SZ];

    /**
     * Using pread() with offset 0 to perform an atomic "seek and read" (the CPU treats it as a single action)
     * This avoids calling lseek to reset the file cursor manually before every sample
     * Reducing syscalls and CPU overhead
     */
    ssize_t bytes_read = pread(net_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read <= 0)
        return;
    buffer[bytes_read] = '\0';

    int active_idx = 0;

    for (int i = 0; i < intf_counter ; i++) {
        char* line = strstr(buffer, interfaces[i].name);
        if (line == NULL) {
            wprintw(plugin_log_win, "[NET] interface %s was not found\n", interfaces[i].name);
            wrefresh(plugin_log_win);
            continue;
        }
        char curr_state[MAX_STATE_BUFFER_SZ];
        bool is_curr_intf_up = is_intf_up(interfaces[i].name, curr_state);

        if (strcmp(curr_state, interfaces[i].last_state) != 0) {
            wprintw(plugin_log_win, "[NET Info]: %s changed from %s to %s\n", 
                    interfaces[i].name, interfaces[i].last_state, curr_state);
            wrefresh(plugin_log_win);

            strncpy(interfaces[i].last_state, curr_state, MAX_STATE_BUFFER_SZ);
            interfaces[i].is_up = is_curr_intf_up;
        }

        if (interfaces[i].is_up) {
            unsigned long long curr_rx, curr_tx, trash;
            char *data_start = strchr(line, ':') + 1;
            int res = sscanf(data_start, "%llu %llu %llu %llu %llu %llu %llu %llu %llu",
            &curr_rx, &trash, &trash, &trash, &trash, &trash, &trash, &trash, &curr_tx);
            if (res == 9) {
                if (!first_run) {
                    float rx_kb = (float)(curr_rx - interfaces[i].last_rx) / 1024;
                    float tx_kb = (float)(curr_tx - interfaces[i].last_tx) / 1024;
                    wprintw(mon_win, "[NET]: %s download %.1f, upload %.1f KB/s\n", interfaces[i].name, rx_kb, tx_kb);
                    wrefresh(mon_win);
                }

                interfaces[i].last_rx = curr_rx;
                interfaces[i].last_tx = curr_tx;
            }
        }
        
        if (active_idx != i) {
            interfaces[active_idx] = interfaces[i];
        }
        active_idx++;
    }
    intf_counter = active_idx;

    if (first_run) {
        wprintw(mon_win, "[NET]: Calculting...\n");
        wrefresh(mon_win);

        first_run = false;
    }
}

void net_cleanup(WINDOW *plugin_log_win) {
    if (net_fd > 0)
        close(net_fd);
    if (interfaces != NULL)
        free(interfaces);
    wprintw(plugin_log_win, "[NET]: Finished cleaning up\n");
    wrefresh(plugin_log_win);
}

plugin_t* get_plugin() {
    static plugin_t p = {
        .name = "net_plugin",
        .init = net_init,
        .run = net_run,
        .cleanup = net_cleanup
    };
    return &p;
}