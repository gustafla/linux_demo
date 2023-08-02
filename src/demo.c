#include "rand.h"
#include "shader.h"
#include "uniforms.h"
#include <GL/gl.h>
#include <GL/glext.h>
#include <SDL2/SDL.h>
#include <assert.h>
#include <stdlib.h>
#include <sync.h>

#define NOISE_SIZE 256

#define GET_VALUE(track_name)                                                  \
    sync_get_val(sync_get_track(rocket, track_name), rocket_row)

static const char *vertex_shader_src =
    "#version 330 core\n"
    "void main() {\n"
    "    vec3 c = vec3(-1,0,1);\n"
    "    vec4 coords[4] = vec4[4](c.xxyz, c.zxyz, c.xzyz, c.zzyz);\n"
    "    gl_Position = coords[gl_VertexID];\n"
    "}\n";

typedef struct {
    GLuint framebuffer;
    GLuint framebuffer_texture;
} fbo_t;

typedef struct {
    int width;
    int height;
    double aspect_ratio;
    int x0;
    int y0;
    int x1;
    int y1;
    GLuint vao;
    program_t effect_program;
    program_t post_program;
    GLuint noise_texture;
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
        SDL_Log("FBO not complete\n");
        return NULL;
    }

    return fbo;
}

static void replace_program(program_t *old, program_t new) {
    if (new.handle) {
        if (old->handle) {
            program_deinit(old);
        }
        *old = new;
    }
#ifndef DEBUG
    else {
        abort();
    }
#endif
}

void demo_reload(demo_t *demo) {
    shader_t vertex_shader = compile_shader(vertex_shader_src, "vert");
    shader_t fragment_shader = compile_shader_file("shaders/shader.frag");

    replace_program(
        &demo->effect_program,
        link_program((shader_t[]){vertex_shader, fragment_shader}, 2));

    shader_t post_shader = compile_shader_file("shaders/post.frag");

    replace_program(&demo->post_program, link_program(
                                             (shader_t[]){
                                                 vertex_shader,
                                                 post_shader,
                                             },
                                             2));

    shader_deinit(&vertex_shader);
    shader_deinit(&fragment_shader);
    shader_deinit(&post_shader);
}

void demo_resize(demo_t *demo, int width, int height) {
    if ((float)width / (float)height > demo->aspect_ratio) {
        demo->y0 = 0;
        demo->y1 = height;
        double adjusted = height * demo->aspect_ratio;
        int remainder = (width - adjusted) / 2;
        demo->x0 = remainder;
        demo->x1 = remainder + adjusted;
    } else {
        demo->x0 = 0;
        demo->x1 = width;
        double adjusted = width / demo->aspect_ratio;
        int remainder = (height - adjusted) / 2;
        demo->y0 = remainder;
        demo->y1 = remainder + adjusted;
    }
}

demo_t *demo_init(int width, int height) {
    demo_t *demo = calloc(1, sizeof(demo_t));
    if (!demo) {
        return NULL;
    }

    demo->width = width;
    demo->height = height;
    demo->aspect_ratio = (double)width / (double)height;
    demo_resize(demo, width, height);

    glGenVertexArrays(1, &demo->vao);
    glBindVertexArray(demo->vao);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    demo_reload(demo);

    demo->post_fb = create_framebuffer(width, height);
    demo->output_fb = create_framebuffer(width, height);

    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &demo->noise_texture);
    glBindTexture(GL_TEXTURE_2D, demo->noise_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, NOISE_SIZE, NOISE_SIZE, 0, GL_RED,
                 GL_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    return demo;
}

static char *rocket_component(const char *name, size_t name_len, char c) {
    static char components[UFM_NAME_MAX + 2];
    assert(name_len < UFM_NAME_MAX);
    memcpy(components, name, name_len);
    components[name_len] = '.';
    components[name_len + 1] = c;
    components[name_len + 2] = 0;
    return components;
}

static void set_rocket_uniforms(program_t *program, struct sync_device *rocket,
                                double rocket_row) {
    for (size_t i = 0; i < program->uniform_count; i++) {
        uniform_t *ufm = program->uniforms + i;

        // Check and adjust for r_ -prefix
        if (ufm->name_len < 3 || ufm->name[0] != 'r' || ufm->name[1] != '_') {
            continue;
        }
        const char *name = ufm->name + 2;
        size_t name_len = ufm->name_len - 2;

        GLuint location = glGetUniformLocation(program->handle, ufm->name);
        switch (ufm->type) {
        case UFM_FLOAT:
            glUniform1f(location, GET_VALUE(name));
            break;
        case UFM_VEC2:
            glUniform2f(location,
                        GET_VALUE(rocket_component(name, name_len, 'x')),
                        GET_VALUE(rocket_component(name, name_len, 'y')));
            break;
        case UFM_VEC3:
            glUniform3f(location,
                        GET_VALUE(rocket_component(name, name_len, 'x')),
                        GET_VALUE(rocket_component(name, name_len, 'y')),
                        GET_VALUE(rocket_component(name, name_len, 'z')));
            break;
        case UFM_VEC4:
            glUniform4f(location,
                        GET_VALUE(rocket_component(name, name_len, 'x')),
                        GET_VALUE(rocket_component(name, name_len, 'y')),
                        GET_VALUE(rocket_component(name, name_len, 'z')),
                        GET_VALUE(rocket_component(name, name_len, 'w')));
            break;
        case UFM_INT:
            glUniform1i(location, (GLint)GET_VALUE(name));
            break;
        case UFM_UNKNOWN:;
        }
    }
}

void demo_render(demo_t *demo, struct sync_device *rocket, double rocket_row) {
    static char noise[NOISE_SIZE * NOISE_SIZE * 4];

    // Early return if shaders are currently unusable
    if (!demo->effect_program.handle || !demo->post_program.handle) {
        return;
    }

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, demo->post_fb->framebuffer);
    glClear(GL_COLOR_BUFFER_BIT);
    glViewport(0, 0, demo->width, demo->height);

    glUseProgram(demo->effect_program.handle);
    set_rocket_uniforms(&demo->effect_program, rocket, rocket_row);
    glUniform1f(
        glGetUniformLocation(demo->effect_program.handle, "u_RocketRow"),
        rocket_row);
    glUniform2f(
        glGetUniformLocation(demo->effect_program.handle, "u_Resolution"),
        demo->width, demo->height);

    glBindVertexArray(demo->vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, demo->output_fb->framebuffer);
    glClear(GL_COLOR_BUFFER_BIT);
    glViewport(0, 0, demo->width, demo->height);

    glUseProgram(demo->post_program.handle);
    set_rocket_uniforms(&demo->post_program, rocket, rocket_row);
    glUniform1f(glGetUniformLocation(demo->post_program.handle, "u_RocketRow"),
                rocket_row);
    glUniform2f(glGetUniformLocation(demo->post_program.handle, "u_Resolution"),
                demo->width, demo->height);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, demo->post_fb->framebuffer_texture);
    glUniform1i(
        glGetUniformLocation(demo->post_program.handle, "u_InputSampler"), 0);
    glActiveTexture(GL_TEXTURE1);
    for (GLsizei i = 0; i < NOISE_SIZE * NOISE_SIZE * 4; i++) {
        noise[i] = rand_xoshiro();
    }
    glBindTexture(GL_TEXTURE_2D, demo->noise_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, NOISE_SIZE, NOISE_SIZE, GL_RGBA,
                    GL_UNSIGNED_BYTE, noise);
    glUniform1i(
        glGetUniformLocation(demo->post_program.handle, "u_NoiseSampler"), 1);
    glUniform1i(glGetUniformLocation(demo->post_program.handle, "u_NoiseSize"),
                NOISE_SIZE);

    glBindVertexArray(demo->vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, demo->output_fb->framebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glBlitFramebuffer(0, 0, demo->width, demo->height, demo->x0, demo->y0,
                      demo->x1, demo->y1, GL_COLOR_BUFFER_BIT, GL_LINEAR);
}

void demo_deinit(demo_t *demo) {
    if (demo) {
        free(demo);
    }
}
