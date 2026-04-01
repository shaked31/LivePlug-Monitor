/*
 * core_engine.c: handles dynamic plugin loading, listens for changes in /bin/plugins dir
*/

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <dlfcn.h>
#include <string.h>
#include <stdbool.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <poll.h>
#include <signal.h>

#include "../include/plugin_api.h"
#include "../include/utils.h"

#define MAX_PLUGIN_NAME_LEN 64
#define MAX_FILENAME_LEN 64
#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUFFER_LEN (1024 * (EVENT_SIZE + MAX_FILENAME_LEN)) // allows the buffer to save up to 1024 events at once


static volatile bool keep_running = true;
static int dashboard_start_line = 0;

/**
 * Signal Handler for SIGINT - Ctrl+C
 * Used in engine_start_monitor
 */
static void handle_sigint(int sig) {
    (void)sig;
    keep_running = false;
}

/**
 * Moves the cursor up and clear the dashboard area before printing a new log
 * Prevents logs to push down the dashboard
 */
static void clear_dashboard_before_log() {
    if (dashboard_start_line > 0) {
        printf("\033[%dA\033[J", dashboard_start_line);
        dashboard_start_line = 0;
    }
    fflush(stdout);
}

/**
 * Linked list of plugin nodes
 */
typedef struct plugin_node {
    char plugin_name[MAX_PLUGIN_NAME_LEN];
    char filename[MAX_FILENAME_LEN];
    plugin_t* curr_plugin;
    void* handle;
    struct plugin_node* next;
} plugin_node_t;
static plugin_node_t* head = NULL;

/**
 * Handles the loading of a plugin
 * Initializes the plugin
 * Add the plugin node to the linked list
 */
static void engine_load_plugin(const char* full_path) {
    void* handle = dlopen(full_path, RTLD_LAZY); // RTLD_LAZY makes function symbols only be resolved when they are called
    if (!handle) {
        fprintf(stderr, "[Core Error]: Couldn't load %s (%s)\n", full_path, dlerror());
        return;
    }

    get_plugin_f get_plugin_func = (get_plugin_f)dlsym(handle, "get_plugin"); // dlsym uses the handle to search for a function in the name defined
    if (!get_plugin_func) {
        fprintf(stderr, "[Core Error]: Symbol 'get_plugin' was not found in %s\n", full_path);
        dlclose(handle);
        return;
    }

    plugin_t* p = get_plugin_func();
    if (p->init() != 0) {
        fprintf(stderr, "[Core Error]: Couldn't initialize plugin '%s'\n", p->name);
        dlclose(handle);
        return;
    }

    plugin_node_t* new_node = (plugin_node_t*)malloc(sizeof(plugin_node_t));
    if (new_node == NULL) {
        fprintf(stderr, "[SYS Error] Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }

    char* filename = get_filename(full_path);
    
    snprintf(new_node->plugin_name, MAX_PLUGIN_NAME_LEN, "%s", p->name);
    snprintf(new_node->filename, MAX_FILENAME_LEN, "%s", filename);
    new_node->curr_plugin = p;
    new_node->handle = handle;

    new_node->next = head;
    head = new_node;

    printf("[Core Info] Successfully loaded plugin %s from the file %s\n", head->plugin_name, head->filename);
}

/**
 * Triggers cleanup for plugin where its file is deleted
 * Deletes the node from the linked list
 * Used in engine_handle_event
 */
static void engine_cleanup_plugin(const char* filename) {
    plugin_node_t *curr = head, *prev = NULL;

    while (curr != NULL) {
        if (strcmp(curr->filename, filename) == 0) {
            curr->curr_plugin->cleanup();
            dlclose(curr->handle);

            if (prev == NULL) // delete the first plugin in the linked list
                head = curr->next;
            else
                prev->next = curr->next;

            printf("[Core Info] Plugin %s was cleaned up\n", curr->plugin_name);
            free(curr);
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}

/**
 * The live dashboard renderer
 * Clears the previous dashboard and runs each plugin from the linked list
 */
static void engine_run_all() {
    if (dashboard_start_line > 0) {
        printf("\033[%dA\033[J", dashboard_start_line);
    }

    if (head == NULL) {
        dashboard_start_line = 0;
        return;
    }

    int curr_count = 0;

    printf("\r\033[K\n"); 
    curr_count++;
    
    printf("\r\033[K--- [Linux Modular Observer - Live] ---\n");
    curr_count++;
    printf("\r\033[K---------------------------------------\n");
    curr_count++;

    plugin_node_t* curr = head;
    while (curr != NULL) {
        printf("\r\033[K");
        curr->curr_plugin->run();
        curr_count++;
        curr = curr->next;
    }

    printf("\r\033[K---------------------------------------\n");
    curr_count++;
    dashboard_start_line = curr_count;

    fflush(stdout);
}

/**
 * Parses inotify events
 * Handles creation, deletion, movement of files in the directory received in the params
 */
static void engine_handle_event(int length, char* buffer, const char* path) {
    int i = 0;
    while (i < length) {
        struct inotify_event *event = (struct inotify_event *)&buffer[i];
        if (event->len) {
            char fullPluginPath[256];
            snprintf(fullPluginPath, sizeof(fullPluginPath), "%s/%s", path, event->name);

            bool is_valid_file = check_file_extention(event->name);
            bool is_create = event->mask & (IN_CLOSE_WRITE | IN_MOVED_TO);
            bool is_delete = event->mask & (IN_DELETE | IN_MOVED_FROM);

            if (is_create) {
                clear_dashboard_before_log();
                printf("[Core Info] The file %s was created\n", event->name);

                if (!is_valid_file) {
                    clear_dashboard_before_log();
                    printf("[Core Warning] This folder is for .so files only, deleting %s\n", event->name);
                    remove(fullPluginPath);
                }
                else {
                    clear_dashboard_before_log();
                    printf("[Core Info] loading plugin from: %s\n", fullPluginPath);
                    engine_load_plugin(fullPluginPath);
                }
            }
            else if (is_delete && is_valid_file) {
                clear_dashboard_before_log();
                printf("[Core Info] The file %s was deleted\n", event->name);
                engine_cleanup_plugin(event->name);
            }
        }
        i += (EVENT_SIZE + event->len); // event->len is the length of the event's name
    }
}

/**
 * Scans the plugin dir and loads all existing Shared Object files using 'engine_load_plugin'
 */
void engine_load_existing_plugins(const char* path) {
    printf("\n");
    fflush(stdout);

    struct dirent *entry;
    DIR *dir = opendir(path);

    if (dir == NULL) {
        fprintf(stderr, "[SYS Error] Couldn't open dir '%s'\n", path);
        return;
    }
    
    printf("[Core Info] loading existing plugins!\n");

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] != '.') {
            char fullPluginPath[512];
            snprintf(fullPluginPath, sizeof(fullPluginPath), "%s/%s", path, entry->d_name);
            bool is_valid_file = check_file_extention(entry->d_name);
            if (!is_valid_file) {
                printf("[Core Warning] This folder is for .so files only, deleting this file\n");
                remove(fullPluginPath);
            }
            engine_load_plugin(fullPluginPath);
        }
    }
    closedir(dir);
}

/**
 * Initializes inotify, uses polling to react to events / update dashboard efficiently
 */
void engine_start_monitor(const char* path) {
    signal(SIGINT, handle_sigint);

    int fd = inotify_init(); // init inotify
    if (fd < 0) {
        fprintf(stderr, "[SYS Error] Couldn't init inotify\n");
        exit(EXIT_FAILURE);
    }
    else {
        printf("[SYS Info] Ready to listen to events\n");
    }
    /*
        watch for creation \ deletion \ movement of files in the folder.
        IN_CLOSE_WRITE makes sure that the program doesnt start to load the .so file before gcc finished writing to it.
    */ 
    int wd = inotify_add_watch(fd, path, IN_CLOSE_WRITE | IN_MOVED_TO | IN_DELETE | IN_MOVED_FROM);

    struct pollfd fds[1]; // 1 source of file descriptor
    fds[0].fd = fd; // the file descriptor
    fds[0].events = POLLIN; // wakes up only if there's an input waiting in this pipeline (the fd)

    char buffer[BUFFER_LEN];

    while (keep_running) {
        int ret = poll(fds, 1, 1000); // wakes after 1 sec or when there's an input waiting in the pipeline

        // the kernel returns more than 0 only if an event happend in inotify
        
        if (ret > 0 && fds[0].revents & POLLIN) {
            while (ret > 0 && fds[0].revents & POLLIN) {
                int length = read(fd, buffer, BUFFER_LEN);
                engine_handle_event(length, buffer, path);
                ret = poll(fds, 1, 10);
            }
            fflush(stdout);
            if (head != NULL)
                engine_run_all();
            continue;
        }
        if (head != NULL)
            engine_run_all();
    }

    // restore_terminal();
    printf("\n[Core Info] Observer shut down. Bye!\n");
    inotify_rm_watch(fd, wd);
    close(fd);
}