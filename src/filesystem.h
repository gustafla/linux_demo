#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <stddef.h>

size_t read_file(const char *filename, char **dst);
char *path_join(const char *path, const char *name);

#endif
