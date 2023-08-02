#ifdef DEBUG

#include <stddef.h>

const unsigned char *filesystem_open(const char *filename, unsigned int *len) {
    return NULL;
}

#else

#include "data.c"
#include <string.h>

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
