#include "util.h"
#include <GL/gl.h>
#include <GL/glext.h>
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <sync.h>

#define GET_VALUE(track_name)                                                  \
    sync_get_val(sync_get_track(rocket, track_name), rocket_row)

static const char *vertex_shader_src =
    "#version 330 core\n"
    "layout(location=0) in vec2 a_Pos;\n"
    "layout(location=1) in vec2 a_TexCoord;\n"
    "out vec2 Pos;\n"
    "out vec2 TexCoord;\n"
    "void main() {\n"
    "    Pos = a_Pos;\n"
    "    TexCoord = a_TexCoord;\n"
    "    gl_Position = vec4(a_Pos, 0., 1.);\n"
    "}\n";

static const GLfloat quad[] = {-1.f, -1., 0., 0., 1.,  -1., 1., 0.,
                               1.,   1.,  1., 1., -1., -1., 0., 0.,
                               1.,   1.,  1., 1., -1., 1.,  0., 1.};

typedef struct {
    int width;
    int height;
    int output_width;
    int output_height;
    GLuint quad_buffer;
    GLuint vao;
    GLuint effect_program;
} demo_t;

demo_t *demo_init(int width, int height) {
    demo_t *demo = calloc(1, sizeof(demo_t));
    if (!demo) {
        return NULL;
    }

    demo->output_width = demo->width = width;
    demo->output_height = demo->height = height;

    glGenBuffers(1, &demo->quad_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, demo->quad_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), (const GLvoid *)quad,
                 GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glGenVertexArrays(1, &demo->vao);
    glBindVertexArray(demo->vao);
    glBindBuffer(GL_ARRAY_BUFFER, demo->quad_buffer);
    size_t stride = sizeof(GLfloat) * 4;
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, NULL);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride,
                          (const void *)(sizeof(GLfloat) * 2));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    GLuint vertex_shader = compile_shader(vertex_shader_src, "vert");
    if (!vertex_shader) {
        return NULL;
    }

    GLuint fragment_shader = compile_shader_file("data/shader.frag");
    if (!fragment_shader) {
        return NULL;
    }

    demo->effect_program =
        link_program(2, (GLuint[]){vertex_shader, fragment_shader});

    return demo;
}

void demo_render(demo_t *demo, struct sync_device *rocket, double rocket_row) {
    glViewport(0, 0, demo->width, demo->height);

    glUseProgram(demo->effect_program);
    glUniform1f(glGetUniformLocation(demo->effect_program, "u_RocketRow"),
                rocket_row);

    glBindVertexArray(demo->vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void demo_deinit(demo_t *demo) {
    if (demo) {
        free(demo);
    }
}
