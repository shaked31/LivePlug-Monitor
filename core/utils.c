#include "../include/utils.h"

// Function that accepts full path to file and returns only the filename
char* get_filename(const char* fullPath) {
    char* filename = strrchr(fullPath, '/');
    if (filename == NULL) {
        perror("Not a valid path");
        return NULL;
    }
    return filename + 1;
}

// Function that accepts full filename and returns true if its a Shared Object file and false otherwise
bool check_file_extention(const char* filename) {
    const char* lastDot = strrchr(filename, '.');
    if (lastDot && strcmp(lastDot, SHARED_OBJ_EXT) == 0) {
        return true;
    }
    return false;
}