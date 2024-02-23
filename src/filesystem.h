#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <stddef.h>

const unsigned char *filesystem_open(const char *filename, unsigned int *len);
size_t read_file(const char *filename, char **dst);

#endif
