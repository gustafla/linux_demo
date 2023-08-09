#ifndef DEMO_H
#define DEMO_H

#include <sync.h>

// Forward declaration so that implementation remains opaque
typedef struct demo_t_ demo_t;

demo_t *demo_init(int width, int height);
void demo_render(demo_t *demo, struct sync_device *rocket, double rocket_row);
void demo_reload(demo_t *demo);
void demo_resize(demo_t *demo, int width, int height);
void demo_deinit(demo_t *demo);

#endif
