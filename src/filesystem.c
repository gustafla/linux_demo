#include <SDL2/SDL_log.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef DEBUG

// This includes a script-generated C source file which contains resources
// such as music .ogg file and shaders.
#include "data.c"
#include <string.h>

// A support struct for reading from static memory as if it was a file
typedef struct {
    const unsigned char *start;
    const unsigned char *at;
    const unsigned char *end;
} mem_file_t;

// This searches for the filename from the generated C source (array called
// data_filenames), and if a match is found, returns the corresponding
// pointer and length.
static const unsigned char *filesystem_open(const char *filename,
                                            unsigned int *len) {
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

// Open a filename, ignores `mode`, the mode is always read-only.
mem_file_t *__wrap_fopen(const char *filename, const char *mode) {
    unsigned int len = 0;
    const unsigned char *data = filesystem_open(filename, &len);
    if (!data) {
        return NULL;
    }

    mem_file_t *io = malloc(sizeof(mem_file_t));
    io->start = io->at = data;
    io->end = data + len;

    return io;
}

// Set file reading position
int __wrap_fseek(mem_file_t *file, long offset, int whence) {
    const unsigned char *const old_pos = file->at;

    switch (whence) {
    case SEEK_SET:
        file->at = file->start + offset;
        break;
    case SEEK_CUR:
        file->at = file->at + offset;
        break;
    case SEEK_END:
        file->at = file->end + offset;
        break;
    default:
        errno = EINVAL;
        return -1;
    }

    if (file->at > file->end) {
        file->at = file->end;
    }
    if (file->at < file->start) {
        file->at = old_pos;
        errno = EINVAL;
        return -1;
    }

    return 0;
}

// Rewind file back to start
void __wrap_rewind(mem_file_t *file) { file->at = file->start; }

// Get file reading position
long __wrap_ftell(mem_file_t *file) { return file->at - file->start; }

// Check if file at end
int __wrap_feof(mem_file_t *file) { return file->at >= file->end; }

// Check if file io encountered an error
int __wrap_ferror(mem_file_t *file) { return 0; }

// Read bytes from a mem_file_t
size_t __wrap_fread(void *buffer, size_t size, size_t count, mem_file_t *file) {
    if (!buffer || !file) {
        return 0;
    }

    size_t available = file->end - file->at;
    if (count * size > available) {
        count = available / size;
    }

    memcpy(buffer, file->at, count * size);
    file->at += count * size;

    return count;
}

// Read a byte from mem_file_t
int __wrap_fgetc(mem_file_t *file) {
    if (file->at >= file->end) {
        return EOF;
    }
    unsigned char c = *(file->at++);
    return (int)c;
}

// Backtrack by a byte
int __wrap_ungetc(int c, mem_file_t *file) {
    if (c == EOF) {
        return EOF;
    }
    file->at--;
    return c;
}

// Close a mem_file_t
int __wrap_fclose(mem_file_t *file) {
    if (file) {
        free(file);
    }
    return 0;
}

#endif

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
