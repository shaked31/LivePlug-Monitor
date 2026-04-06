# 🔌 LivePlug — Dynamic Plugin Monitor

LivePlug Monitor is a lightweight, high-performance Linux system utility written in C. It provides a real-time, extensible dashboard for monitoring system resources (like CPU and Memory) through a dynamic plugin architecture.
---

## Features

- **Hot-loading** — plugins are loaded and unloaded dynamically using `dlopen`/`dlclose`
- **Live dashboard** — refreshes in-place in the terminal using ANSI escape codes
- **inotify-based watching** — reacts to file creation, deletion, and moves in the plugins directory
- **Polling architecture** — uses `poll()` for efficient event handling without busy-waiting
- **Graceful shutdown** — catches `SIGINT` (Ctrl+C) and cleans up all plugins before exiting
- **Bundled plugins** — ships with CPU and memory monitors that read from `/proc`

---

## Project Structure

```
.
├── core/
│   ├── main.c            # Entry point
│   ├── core_engine.c     # Plugin loading, inotify, dashboard loop
│   ├── ui_manager.c      # UI Management using ncurses (thread-safe printing)
│   └── utils.c           # Utility functions (filename extraction, file validation)
├── plugins/
│   ├── cpu_monitor.c     # CPU usage plugin (reads /proc/stat)
│   ├── mem_monitor.c     # Memory usage plugin (reads /proc/meminfo)
│   ├── net_monitor.c     # Network usage plugin (reads /proc/net/dev & /sys/class/net)
│   ├── thermal_monitor.c # Thermal/CPU temperature plugin (reads /sys/class/thermal)
│   └── hello_plugin.c    # Minimal example plugin
├── include/
│   ├── common_defs.h     # Global constants (MAX_PLUGINS, MAX_PLUGIN_NAME_LEN)
│   ├── plugin_api.h      # Plugin interface definition (plugin_t struct)
│   ├── core_engine.h     # Engine function declarations (plugin loading/cleanup)
│   ├── ui_manager.h      # UI Management declarations (safe_print, window getters)
│   └── utils.h           # Utility function declarations
├── build/                # Compiled core binary
└── bin/
    └── plugins/          # Compiled .so files (watched directory)
```

---

## Getting Started

### Requirements

- Linux (uses `inotify` and `/proc`)
- GCC
- GNU Make
 - libncurses-dev (run 'sudo apt install libncurses-dev')
### Build

```bash
# Build everything (core + all plugins)
make

# Build only the core engine
make core

# Build only the plugins
make plugins
```

### Run

```bash
./build/core
```

The engine will scan `./bin/plugins/` for existing `.so` files, load them, and start watching for changes.

---

## How It Works

```
┌─────────────────────────────────────────────────┐
│                  core engine                    │
│                                                 │
│  1. Scan bin/plugins/ → load existing .so files │
│  2. inotify watch on bin/plugins/               │
│  3. poll() loop (1s timeout)                    │
│     ├── Event? → load or unload plugin          │
│     └── Timeout? → refresh dashboard            │
└─────────────────────────────────────────────────┘
```

Each plugin is a shared object exposing a single `get_plugin()` function that returns a `plugin_t` struct with three function pointers: `init`, `run`, and `cleanup`.

Loaded plugins are stored in a **singly linked list**. The dashboard iterates the list and calls `run()` on each plugin every cycle in parallel threads.

---

## Writing a Plugin

### Plugin API

Every plugin must implement the `plugin_t` interface (defined in `plugin_api.h`):

```c
typedef struct {
    const char* name;  // Plugin identifier
    int (*init)(WINDOW *plugin_log_win);  // Called once at load
    void (*run)(WINDOW *mon_win, WINDOW *plugin_log_win, int monitor_row_idx);  // Called every refresh cycle
    void (*cleanup)(WINDOW *plugin_log_win);  // Called before plugin unload
} plugin_t;
```

### Implementation Example

Implement the three callback functions and export `get_plugin()`:

```c
#include "plugin_api.h"
#include "ui_manager.h"  // For safe_print()

int my_init(WINDOW *plugin_log_win) {
    safe_print(plugin_log_win, -1, "[MY PLUGIN] Initialized\n");
    return 0;  // return non-zero to abort loading
}

void my_run(WINDOW *mon_win, WINDOW *plugin_log_win, int monitor_row_idx) {
    // Print to dashboard at the row assigned to this plugin
    safe_print(mon_win, monitor_row_idx, "[MY PLUGIN] Status here\n");
    
    // Or print to log window if needed
    safe_print(plugin_log_win, -1, "[MY PLUGIN] Debug info\n");
}

void my_cleanup(WINDOW *plugin_log_win) {
    safe_print(plugin_log_win, -1, "[MY PLUGIN] Cleaned up\n");
}

plugin_t* get_plugin() {
    static plugin_t p = {
        .name    = "my_plugin",
        .init    = my_init,
        .run     = my_run,
        .cleanup = my_cleanup
    };
    return &p;
}
```

### Key Points

- **Always use `safe_print()`** instead of `wprintw()` — it's thread-safe and handles ncurses operations atomically
- **`monitor_row_idx`** is the row assigned to your plugin on the dashboard. Use it when printing to `mon_win`
- **`plugin_log_win`** is for debug/status logs of plugins; use row `-1` to always append
- **Multiple plugins run in parallel** — the core uses `pthread` to call `run()` on each plugin simultaneously
- **Keep `run()` fast** — it's called every refresh cycle (~1 second)

### Compilation

Compile as a shared object and drop it into `bin/plugins/`:

```bash
gcc -Wall -Wextra -fPIC -shared -lncurses -lpthread -I./include plugins/my_plugin.c -o bin/plugins/my_plugin.so
```

Note: Link with `-lpthread` since plugins may call thread-safe functions like `safe_print()` which uses `pthread_mutex` internally.

The engine will detect the new file and load it automatically.

---

## Common Definitions

### `common_defs.h`

Defines global constants used across the project:

```c
#define MAX_PLUGINS 8              // Maximum number of plugins that can be loaded simultaneously
#define MAX_PLUGIN_NAME_LEN 64     // Maximum length of a plugin's name string
```

These limits prevent unbounded memory allocation and ensure predictable resource usage.

---

## Bundled Plugins

| Plugin | Source | Description |
|---|---|---|
| `cpu_monitor` | `/proc/stat` | CPU usage % (delta between samples) |
| `mem_monitor` | `/proc/meminfo` | RAM used / total in GB |
| `net_monitor` | `/proc/net/dev & /sys/class/net` | download, upload in KB/s |
| `thermal_monitor` | `/sys/class/thermal` | CPU package temperature in °C |
| `hello_plugin` | — | Minimal example plugin |

---

## Makefile Targets

| Target | Description |
|---|---|
| `make` / `make all` | Build core + all plugins |
| `make core` | Build only the core engine |
| `make plugins` | Build only the `.so` plugins |
| `make clean` | Remove all build artifacts |
| `make pclean` | Remove only compiled plugins |
| `make bclean` | Remove only the core binary |

---

## Notes

- Only `.so` files are allowed in `bin/plugins/`. Any other file placed there will be automatically deleted with a warning.
- CPU usage is computed as a **delta** between two samples, so the first reading always shows `Calculating...`.
- Both the CPU and memory plugins use `pread()` with offset 0 to avoid manual `lseek` calls between samples.
