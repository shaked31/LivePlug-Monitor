#include "../include/utils.h"

char* get_filename(const char* fullPath) {
    char* filename = strrchr(fullPath, '/');
    if (filename == NULL) {
        perror("Not a valid path");
        return NULL;
    }
    return filename + 1;
}

bool check_file_extention(const char* filename) {
    const char* lastDot = strrchr(filename, '.');
    if (lastDot && strcmp(lastDot, SHARED_OBJ_EXT) == 0) {
        return true;
    }
    return false;
}