#ifndef DEMO_H
#define DEMO_H

typedef struct demo_t_ demo_t;

demo_t *demo_init(void);
void demo_deinit(demo_t *demo);
void demo_render(demo_t *demo);

#endif
