#define _XOPEN_SOURCE 500

#include "../include/plugin_api.h"
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

int cpu_init() {
    cpu_fd = open(PATH_TO_CPU_FILE, O_RDONLY);
    if (cpu_fd < 0) {
        fprintf(stderr, "Couldn't open /proc/stat\n");
        return EXIT_FAILURE;
    }
    printf("CPU Monitor Plugin initialized\n");
    first_run = true;
    last_total = 0;
    last_idle = 0;

    return EXIT_SUCCESS;
}

void cpu_run() {
    if (cpu_fd < 0) 
        return;

    char buffer[MAX_BUFFER_SZ];
    ssize_t bytes_read = pread(cpu_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read <= 0)
        return;
    buffer[bytes_read] = '\0';

    long long user, nice, system, idle, iowait, irq, softirq, steal;
    

    if (sscanf(buffer, "cpu %lld %lld %lld %lld %lld %lld %lld %lld",
                &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal) < 4) // fscanf returns the number of variables it managed to read and write into successfully. 4 - the basic variables we need user,nice,system,idle
                return;

    long long curr_idle = idle + iowait;
    long long curr_non_idle = user + nice + system + irq + softirq + steal;
    long long curr_total = curr_idle + curr_non_idle;

    if (!first_run) {
        long long total_diff = curr_total - last_total;
        long long idle_diff = curr_idle - last_idle;

        if (total_diff > 0) {
            float usage = (float)(total_diff - idle_diff) / total_diff * 100.0;
            if (usage > 100)
                usage = 100;
            else if (usage < 0)
                usage = 0;

            printf("[CPU] Usage: %.2f%%\n", usage);
            fflush(stdout);
        }
    }
    else {
        printf("[CPU] Calculating...\n");
        first_run = false;
    }

    last_total = curr_total;
    last_idle = curr_idle;
}

void cpu_cleanup() {
    if (cpu_fd >= 0)
        close(cpu_fd);
}


plugin_t* get_plugin() {
    static plugin_t p = {
        .name = "cpu_plugin",
        .init = cpu_init,
        .run = cpu_run,
        .cleanup = cpu_cleanup
    };
    return &p;
}