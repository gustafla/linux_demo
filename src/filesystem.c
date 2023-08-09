#ifdef DEBUG

#include <stddef.h>

// In debug builds, because empty units are forbidden by C standard, we have
// this no-op function.
const unsigned char *filesystem_open(const char *filename, unsigned int *len) {
    return NULL;
}

#else

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

#endif
