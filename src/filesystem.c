#ifdef DEBUG

#include <SDL2/SDL_log.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

const unsigned char *filesystem_open(const char *filename, unsigned int *len) {
    return NULL;
}

// This read_file implementation completely reads a file from disk.
// Returns the number of bytes in the file, or 0 if the read failed.
// Changes *dst via pointer dereference. If successful *dst will point to the
// data, or sets it to NULL if unsuccessful.
size_t read_file(const char *filename, char **dst) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        goto cleanup;
    }

    fseek(file, 0, SEEK_END);
    size_t len = ftell(file);
    if (!len) {
        goto cleanup;
    }

    *dst = (char *)malloc(len);
    fseek(file, 0, SEEK_SET);

    size_t read = fread(*dst, sizeof(char), len, file);
    fclose(file);
    if (read != len)
        goto cleanup;

    return len;

cleanup:
    if (*dst) {
        free(*dst);
        *dst = NULL;
    }
    SDL_Log("Failed to read file %s\n", filename);
    return 0;
}

#else // ----------------------------------------------------------------------

// This includes a script-generated C source file which contains resources
// such as music .ogg file and shaders.
#include "data.c"
#include <string.h>

// This searches for the filename from the generated C source (array called
// data_filenames), and if a match is found, returns the corresponding
// pointer and length.
const unsigned char *filesystem_open(const char *filename, unsigned int *len) {
    const char *search = data_filenames;
    for (int i = 0; *search != 0; i++) {
        if (strcmp(filename, search) == 0) {
            *len = data_lens[i];
            return data_ptrs[i];
        }

        search = strchr(search, 0) + 1;
    }

    return NULL;
}

// This read_file implementation gives access to a file in the executable
// via `filesystem_open`.
// Returns the number of bytes in the file, or 0 if the read failed.
// Changes *dst via pointer dereference. If successful *dst will point to the
// data, or sets it to NULL if unsuccessful.
size_t read_file(const char *filename, char **dst) {
    unsigned int len = 0;
    const unsigned char *data = filesystem_open(filename, &len);
    if (!data) {
        SDL_Log("Cannot find file %s\n", filename);
        *dst = NULL;
        return 0;
    }

    *dst = (char *)data;
    return len;
}

#endif

char *path_join(const char *path, const char *name) {
    size_t len_path = strlen(path), len_name = strlen(name);

    // Allocate buffer for path + '/' + name + \0
    char *fullpath = malloc(len_path + 1 + len_name + 1);
    if (!fullpath) {
        return NULL;
    }

    // Copy path to buffer
    memcpy(fullpath, path, len_path);
    // Insert /
    fullpath[len_path] = '/';
    // Copy name after path and /
    memcpy(fullpath + len_path + 1, name, len_name);
    // Insert null-terminator sentinel
    fullpath[len_path + len_name + 1] = '\0';

    return fullpath;
}
