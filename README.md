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
│   └── utils.c           # Filename helpers
├── plugins/
│   ├── cpu_monitor.c     # CPU usage plugin (reads /proc/stat)
│   ├── mem_monitor.c     # Memory usage plugin (reads /proc/meminfo)
│   └── hello_plugin.c    # Minimal example plugin
├── include/
│   ├── plugin_api.h      # Plugin interface definition
│   ├── core_engine.h     # Engine function declarations
│   └── utils.h           # Utility function declarations
└── bin/
    └── plugins/          # Compiled .so files (watched directory)
```

---

## Getting Started

### Requirements

- Linux (uses `inotify` and `/proc`)
- GCC
- GNU Make

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

Loaded plugins are stored in a **singly linked list**. The dashboard iterates the list and calls `run()` on each plugin every cycle.

---

## Writing a Plugin

Implement the `plugin_t` interface and export `get_plugin()`:

```c
#include "plugin_api.h"
#include <stdio.h>

int my_init() {
    printf("My plugin initialized\n");
    return 0; // return non-zero to abort loading
}

void my_run() {
    printf("[MY PLUGIN] Hello from the dashboard!\n");
}

void my_cleanup() {
    printf("My plugin cleaned up\n");
}

plugin_t* get_plugin() {
    static plugin_t p = {
        .name    = "my_plugin",
        .init    = my_init,
        .run     = my_run,
        .cleanup = my_cleanup,
    };
    return &p;
}
```

Compile as a shared object and drop it into `bin/plugins/`:

```bash
gcc -Wall -fPIC -shared my_plugin.c -o bin/plugins/my_plugin.so
```

The engine will detect the new file and load it automatically.

---

## Bundled Plugins

| Plugin | Source | Description |
|---|---|---|
| `cpu_monitor` | `/proc/stat` | CPU usage % (delta between samples) |
| `mem_monitor` | `/proc/meminfo` | RAM used / total in GB |
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
