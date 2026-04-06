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
#include <pthread.h>

#include "../include/plugin_api.h"
#include "../include/utils.h"
#include "../include/ui_manager.h"
#include "../include/common_defs.h"

#define MAX_FILENAME_LEN 64
#define EVENT_SIZE (sizeof(struct inotify_event))
#define INOTIFY_BUFFER_LEN (1024 * (EVENT_SIZE + MAX_FILENAME_LEN)) // allows the buffer to save up to 1024 events at once

/**
 * Linked list of plugin nodes
 */
typedef struct plugin_node {
    char plugin_name[MAX_PLUGIN_NAME_LEN];
    char filename[MAX_FILENAME_LEN];
    plugin_t* curr_plugin;
    void* handle;
    struct plugin_node* next;
    int monitor_row_idx;
} plugin_node_t;

/**
 * data structure of args to pass through thread to plugin
 */
typedef struct {
    plugin_node_t* node;
    WINDOW* mon_win;
    WINDOW* plugin_log_win;
} core_thread_args_t;

static plugin_node_t* head = NULL;
static pthread_mutex_t list_mutex = PTHREAD_MUTEX_INITIALIZER;
static volatile bool keep_running = true;
static int next_available_row = 2;
WINDOW *mon_win = NULL, *plugin_log_win = NULL;

/**
 * Signal Handler for SIGINT - Ctrl+C
 * Used in engine_start_monitor
 */
static void handle_sigint(int sig) {
    (void)sig;
    keep_running = false;
}


static void* plugin_thread_routine(void* ptr) {
    core_thread_args_t* args = (core_thread_args_t*)ptr;
    args->node->curr_plugin->run(args->mon_win, args->plugin_log_win, args->node->monitor_row_idx);

    free(args);
    return NULL;
}

/**
 * Handles the loading of a plugin
 * Initializes the plugin
 * Add the plugin node to the linked list
 */
static void engine_load_plugin(const char* full_path) {
    void* handle = dlopen(full_path, RTLD_LAZY); // RTLD_LAZY makes function symbols only be resolved when they are called
    if (!handle) {
        ui_log("[CORE Error] Couldn't load %s (%s)\n", full_path, dlerror());
        return;
    }

    get_plugin_f get_plugin_func = (get_plugin_f)dlsym(handle, "get_plugin"); // dlsym uses the handle to search for a function in the name defined
    if (!get_plugin_func) {
        ui_log("[CORE Error] Symbol 'get_plugin' was not found in %s\n", full_path);
        dlclose(handle);
        return;
    }

    plugin_t* p = get_plugin_func();
    if (p->init(plugin_log_win) != 0) {
        ui_log("[CORE Error] Couldn't initialize plugin '%s'\n", p->name);
        dlclose(handle);
        return;
    }

    plugin_node_t* new_node = (plugin_node_t*)malloc(sizeof(plugin_node_t));
    if (new_node == NULL) {
        ui_log("[CORE Error]: Memory allocation failed\n");
        ui_cleanup();
        exit(EXIT_FAILURE);
    }

    char* filename = get_filename(full_path);
    
    snprintf(new_node->plugin_name, MAX_PLUGIN_NAME_LEN, "%s", p->name);
    snprintf(new_node->filename, MAX_FILENAME_LEN, "%s", filename);
    new_node->curr_plugin = p;
    new_node->handle = handle;
    new_node->monitor_row_idx = next_available_row++;
    new_node->next = head;
    head = new_node;

    ui_log("[CORE Info] Successfully loaded plugin %s from the file %s\n", head->plugin_name, head->filename);
}

/**
 * Triggers cleanup for plugin where its file is deleted
 * Deletes the node from the linked list
 * Used in engine_handle_event
 */
static void engine_cleanup_plugin(const char* filename) {
    pthread_mutex_lock(&list_mutex);
    plugin_node_t *curr = head, *prev = NULL;
    while (curr != NULL) {
        if (strcmp(curr->filename, filename) == 0) {
            curr->curr_plugin->cleanup(plugin_log_win);
            dlclose(curr->handle);

            if (prev == NULL) // delete the first plugin in the linked list
                head = curr->next;
            else
                prev->next = curr->next;

            ui_log("[CORE Info] Plugin %s was cleaned up\n", curr->plugin_name);
            free(curr);
            ui_clear_monitor();
            break;
        }
        prev = curr;
        curr = curr->next;
    }

    int curr_row = 2; // 0 and 1 are the title
    plugin_node_t* temp = head;
    while (temp != NULL) {
        temp->monitor_row_idx = curr_row++;
        temp = temp->next;
    }

    next_available_row = curr_row;
    pthread_mutex_unlock(&list_mutex);
}

static void engine_cleanup_all_plugins() {
    plugin_node_t *curr = head, *next;
    while (curr != NULL) {
        next = curr->next;
        curr->curr_plugin->cleanup(plugin_log_win);
        dlclose(curr->handle);
        free(curr);
        curr = next;
    }
    head = NULL;
}

/**
 * The live dashboard renderer
 * Clears the previous dashboard and runs each plugin from the linked list
 */
static void engine_run_all() {
    ui_clear_monitor();

    safe_print(mon_win, 0, "------------ [ Live-Plug Monitor ] ------------\n");

    if (head != NULL)
        safe_print(mon_win, 1, "-----------------------------------------------\n");

    else {
        safe_print(mon_win, 1, "----------- No active plugins found -----------\n");
        safe_print(mon_win, 2, "-----------------------------------------------\n");
        ui_refresh_monitor();
        return;
    }

    pthread_t threads[MAX_PLUGINS];
    int thread_count = 0;

    pthread_mutex_lock(&list_mutex);
    plugin_node_t* curr = head;
    while (curr != NULL && thread_count < MAX_PLUGINS) {
        core_thread_args_t* args = (core_thread_args_t*)malloc(sizeof(core_thread_args_t));
        args->node = curr;
        args->mon_win = mon_win;
        args->plugin_log_win = plugin_log_win;
        
        pthread_create(&threads[thread_count++], NULL, plugin_thread_routine, args);
        curr = curr->next;
    }
    pthread_mutex_unlock(&list_mutex);

    for (int i = 0 ; i < thread_count ; i++) {
        pthread_join(threads[i], NULL);
    }
    safe_print(mon_win, next_available_row, "-----------------------------------------------\n");
    
    ui_refresh_monitor();
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
                ui_log("[CORE Info] The file %s was created\n", event->name);

                if (!is_valid_file) {
                    ui_log("[CORE Warning] This folder is for .so files only, ignoring %s\n", event->name);
                }
                else {
                    ui_log("[CORE Info] loading plugin from: %s\n", fullPluginPath);
                    engine_load_plugin(fullPluginPath);
                }
            }
            else if (is_delete && is_valid_file) {
                ui_log("[CORE Info] The file %s was deleted\n", event->name);
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
    ui_init();
    mon_win = ui_get_monitor_win();
    plugin_log_win = ui_get_plugin_log_win();

    // If UI initialization failed (e.g., terminal too small), exit cleanly
    if (mon_win == NULL || plugin_log_win == NULL) {
        exit(EXIT_SUCCESS);
    }

    struct dirent *entry;
    DIR *dir = opendir(path);

    if (dir == NULL) {
        ui_log("[CORE Error] Couldn't open dir '%s'\n", path);
        return;
    }
    
    ui_log("[CORE Info] loading existing plugins!\n");

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] != '.') {
            char fullPluginPath[512];
            snprintf(fullPluginPath, sizeof(fullPluginPath), "%s/%s", path, entry->d_name);
            bool is_valid_file = check_file_extention(entry->d_name);
            if (!is_valid_file) {
                ui_log("[CORE Warning] This folder is for .so files only, ignoring this file\n");
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
        ui_log("[CORE Error] Couldn't init inotify\n");
        ui_cleanup();
        exit(EXIT_FAILURE);
    }
    /*
        watch for creation \ deletion \ movement of files in the folder.
        IN_CLOSE_WRITE makes sure that the program doesnt start to load the .so file before gcc finished writing to it.
    */ 
    int wd = inotify_add_watch(fd, path, IN_CLOSE_WRITE | IN_MOVED_TO | IN_DELETE | IN_MOVED_FROM);
    ui_log("[CORE Info] Ready to listen to events\n");

    struct pollfd fds[1]; // 1 source of file descriptor
    fds[0].fd = fd; // the file descriptor
    fds[0].events = POLLIN; // wakes up only if there's an input waiting in this pipeline (the fd)

    char buffer[INOTIFY_BUFFER_LEN];

    while (keep_running) {
        int ret = poll(fds, 1, 1000); // wakes after 1 sec or when there's an input waiting in the pipeline
        
        // the kernel returns more than 0 only if an event happend in inotify
        
        if (ret > 0 && fds[0].revents & POLLIN) {
            while (ret > 0 && fds[0].revents & POLLIN) {
                int length = read(fd, buffer, INOTIFY_BUFFER_LEN);
                engine_handle_event(length, buffer, path);
                ret = poll(fds, 1, 10);
            }
            engine_run_all();
        }
        if (head != NULL)
            engine_run_all();
    }

    engine_cleanup_all_plugins();
    printf("\n[CORE Info] Observer shut down. Bye!\n");
    ui_cleanup();
    inotify_rm_watch(fd, wd);
    close(fd);
}