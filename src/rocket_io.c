#include "filesystem.h"
#include "sync.h"
#include <stdlib.h>
#include <string.h>

#ifndef DEBUG

typedef struct {
    const unsigned char *data;
    const unsigned char *end;
} rocket_io_t;

void *rocket_open(const char *filename, const char *mode) {
    unsigned int len = 0;
    const unsigned char *data = filesystem_open(filename, &len);
    if (!data) {
        return NULL;
    }

    rocket_io_t *io = malloc(sizeof(rocket_io_t));
    io->data = data;
    io->end = data + len;

    return io;
}

size_t rocket_read(void *buffer, size_t size, size_t count, void *rocket_io) {
    if (!buffer || !rocket_io) {
        return 0;
    }

    rocket_io_t *io = rocket_io;
    size_t request = size * count;
    size_t available = io->end - io->data;
    size_t giving = request > available ? available : request;

    memcpy(buffer, io->data, giving);
    io->data += giving;

    return giving;
}

int rocket_close(void *rocket_io) {
    if (rocket_io) {
        free(rocket_io);
    }
    return 0;
}

struct sync_io_cb rocket_iocb = {
    .open = rocket_open, .read = rocket_read, .close = rocket_close};

#else // !defined(DEBUG)

#include <stdio.h>

struct sync_io_cb rocket_iocb = {
    .open = (void *(*)(const char *, const char *))fopen,
    .read = (size_t(*)(void *, size_t, size_t, void *))fread,
    .close = (int (*)(void *))fclose};

#endif // defined(DEBUG)
