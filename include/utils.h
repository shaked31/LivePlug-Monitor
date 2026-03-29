#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#define SHARED_OBJ_EXT ".so"

char* get_filename(const char* fullPath);
bool check_file_extention(const char* filename);

#endif