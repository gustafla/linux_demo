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
    GLuint framebuffer;
    GLuint framebuffer_texture;
} fbo_t;

typedef struct {
    int width;
    int height;
    int output_width;
    int output_height;
    GLuint quad_buffer;
    GLuint vao;
    GLuint effect_program;
    GLuint post_program;
    fbo_t *post_fb;
    fbo_t *output_fb;
} demo_t;

static fbo_t *create_framebuffer(GLsizei width, GLsizei height) {
    fbo_t *fbo = malloc(sizeof(fbo_t));
    if (!fbo) {
        return NULL;
    }

    glGenFramebuffers(1, &fbo->framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo->framebuffer);
    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &fbo->framebuffer_texture);
    glBindTexture(GL_TEXTURE_2D, fbo->framebuffer_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA,
                 GL_HALF_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           fbo->framebuffer_texture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "FBO not complete\n");
        return NULL;
    }

    return fbo;
}

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

    GLuint post_shader = compile_shader_file("data/post.frag");
    if (!post_shader) {
        return NULL;
    }

    demo->post_program =
        link_program(2, (GLuint[]){vertex_shader, post_shader});

    demo->post_fb = create_framebuffer(width, height);
    demo->output_fb = create_framebuffer(width, height);

    return demo;
}

void demo_render(demo_t *demo, struct sync_device *rocket, double rocket_row) {
    glBindFramebuffer(GL_FRAMEBUFFER, demo->post_fb->framebuffer);
    glClear(GL_COLOR_BUFFER_BIT);
    glViewport(0, 0, demo->width, demo->height);

    glUseProgram(demo->effect_program);
    glUniform1f(glGetUniformLocation(demo->effect_program, "u_RocketRow"),
                rocket_row);

    glBindVertexArray(demo->vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glBindFramebuffer(GL_FRAMEBUFFER, demo->output_fb->framebuffer);
    glClear(GL_COLOR_BUFFER_BIT);
    glViewport(0, 0, demo->width, demo->height);

    glUseProgram(demo->post_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, demo->post_fb->framebuffer_texture);
    glUniform1i(glGetUniformLocation(demo->post_program, "u_InputSampler"), 0);
    glUniform1f(glGetUniformLocation(demo->post_program, "u_RocketRow"),
                rocket_row);

    glBindVertexArray(demo->vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, demo->output_fb->framebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    // glClear(GL_COLOR_BUFFER_BIT);
    glBlitFramebuffer(0, 0, demo->width, demo->height, 0, 0, demo->output_width,
                      demo->output_height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
}

void demo_resize(demo_t *demo, int width, int height) {
    demo->output_width = width;
    demo->output_height = height;
}

void demo_deinit(demo_t *demo) {
    if (demo) {
        free(demo);
    }
}
