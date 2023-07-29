#include <GL/gl.h>
#include <GL/glext.h>
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <sync.h>

#define GET_VALUE(track_name)                                                  \
    sync_get_val(sync_get_track(rocket, track_name), rocket_row)

typedef struct {
    GLuint vertex_shader;
} demo_t;

demo_t *demo_init(void) {
    demo_t *demo = calloc(1, sizeof(demo_t));
    if (!demo) {
        return NULL;
    }

    return demo;
}

void demo_render(demo_t *demo, struct sync_device *rocket, double rocket_row) {
    SDL_Log("%f\n", GET_VALUE("test"));
}

void demo_deinit(demo_t *demo) {
    if (demo) {
        free(demo);
    }
}
