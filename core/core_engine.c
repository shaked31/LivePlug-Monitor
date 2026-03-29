/*
 * core_engine.c: handles plugins load, listens for changes in /bin/plugins dir
*/

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <dlfcn.h>
#include <string.h>
#include <stdbool.h>
#include <sys/inotify.h>
#include <unistd.h>

#include "../include/plugin_api.h"
#include "../include/utils.h"

#define MAX_PLUGIN_NAME_LEN 64
#define MAX_FILENAME_LEN 64
#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUFFER_LEN (1024 * (EVENT_SIZE + MAX_FILENAME_LEN)) // allows the buffer to save up to 1024 events at once


typedef struct plugin_node {
    char plugin_name[MAX_PLUGIN_NAME_LEN];
    char filename[MAX_FILENAME_LEN];
    plugin_t* curr_plugin;
    void* handle;
    struct plugin_node* next;
} plugin_node_t;
static plugin_node_t* head = NULL;


static void engine_load_plugin(const char* full_path) {
    void* handle = dlopen(full_path, RTLD_LAZY); // RTLD_LAZY makes function symbols only be resolved when they are called
    if (!handle) {
        fprintf(stderr, "[Core Error]: Couldn't load %s (%s)\n", full_path, dlerror());
        exit(EXIT_FAILURE);
    }

    get_plugin_f get_plugin_func = (get_plugin_f)dlsym(handle, "get_plugin"); // dlsym uses the handle to search for a function in the name defined
    if (!get_plugin_func) {
        fprintf(stderr, "[Core Error]: Symbol not found in %s\n", full_path);
        dlclose(handle);
        exit(EXIT_FAILURE);
    }

    plugin_t* p = get_plugin_func();
    printf("\t");
    p->init();
    printf("\t");
    p->run();

    plugin_node_t* new_node = malloc(sizeof(plugin_node_t));
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

    printf("[Core Info] loading plugin %s from the file %s\n", head->plugin_name, head->filename);

    return EXIT_SUCCESS;
}

void engine_load_existing_plugins(const char* path) {
    struct dirent *de;
    DIR *dr = opendir(path);

    if (dr == NULL) {
        fprintf(stderr, "[SYS Error] Couldn't open dir '%s'\n", path);
        return;
    }
    
    printf("[Core Info] loading existing plugins!\n");
    while ((de = readdir(dr)) != NULL) {
        if (de->d_name[0] != '.') {
            char fullPluginPath[512];
            snprintf(fullPluginPath, sizeof(fullPluginPath), "%s/%s", path, de->d_name);
            if (!check_file_extention(de->d_name)) {
                printf("[Core Error] This folder is for .so files only\n");
                remove(fullPluginPath);
            }
            engine_load_plugin(fullPluginPath);
        }
    }
    closedir(dr);
}

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

void engine_start_monitor(const char* path) {
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

    char buffer[BUFFER_LEN];

    while (1) {
        int length = read(fd, buffer, BUFFER_LEN);
        int i = 0;

        while (i < length) {
            struct inotify_event *event = (struct inotify_event *)&buffer[i];
            printf("[Core Info] Received event %d, %s\n", event->mask, event->name);
            if (event->len) {
                char fullPluginPath[256];
                snprintf(fullPluginPath, sizeof(fullPluginPath), "%s/%s", path, event->name);
                if (event->mask & (IN_CLOSE_WRITE | IN_MOVED_TO)) {
                    printf("[Core Info] The file %s was created\n", event->name);
                    if (!check_file_extention(event->name)) {
                        remove(fullPluginPath);
                        printf("[Core Info] This folder is for .so files only\n");
                    }
                    else {
                        printf("[Core Info] loading plugin from: %s\n", fullPluginPath);
                        engine_load_plugin(fullPluginPath);
                        // plugin->run();
                    }
                }
                else if (event->mask & (IN_DELETE | IN_MOVED_FROM)) {
                    printf("[Core Info] The file %s was deleted\n", event->name);
                    engine_cleanup_plugin(event->name);
                }
            }
            i += (EVENT_SIZE + event->len); // event->len is the length of the event's name
        }
    }

    inotify_rm_watch(fd, wd);
    close(fd);
}