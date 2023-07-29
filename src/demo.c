#include "util.h"
#include <GL/gl.h>
#include <GL/glext.h>
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <sync.h>

#define GET_VALUE(track_name)                                                  \
    sync_get_val(sync_get_track(rocket, track_name), rocket_row)

static const char *vertex_shader = "#version 330 core\n"
                                   "void main() {\n"
                                   "}\n";

typedef struct {
    int width;
    int height;
    int output_width;
    int output_height;
    GLuint vertex_shader;
} demo_t;

demo_t *demo_init(int width, int height) {
    demo_t *demo = calloc(1, sizeof(demo_t));
    if (!demo) {
        return NULL;
    }

    demo->output_width = demo->width = width;
    demo->output_height = demo->height = height;
    demo->vertex_shader = compile_shader(vertex_shader, "vert", NULL);

    return demo;
}

void demo_render(demo_t *demo, struct sync_device *rocket, double rocket_row) {
    glViewport(0, 0, demo->width, demo->height);
}

void demo_deinit(demo_t *demo) {
    if (demo) {
        free(demo);
    }
}
