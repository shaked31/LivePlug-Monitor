#ifndef PLUGIN_API_H
#define PLUGIN_API_H

typedef struct {
    const char* name;
    int (*init)();
    void (*run)();
    void (*cleanup)();
} plugin_t;

typedef plugin_t* (*get_plugin_f)();

#endif