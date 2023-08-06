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
#define FBS 3
#define QUARTER_FBS 2

#define GET_VALUE(track_name)                                                  \
    sync_get_val(sync_get_track(rocket, track_name), rocket_row)

static const char *vertex_shader_src =
    "out vec2 FragCoord;\n"
    "void main() {\n"
    "    vec2 c = vec2(-1, 1);\n"
    "    vec4 coords[4] = vec4[4](c.xxyy, c.yxyy, c.xyyy, c.yyyy);\n"
    "    FragCoord = coords[gl_VertexID].xy;\n"
    "    gl_Position = coords[gl_VertexID];\n"
    "}\n";

typedef struct {
    GLuint framebuffer;
    GLuint texture;
    GLsizei width;
    GLsizei height;
} fbo_t;

typedef struct {
    int width;
    int height;
    double aspect_ratio;
    int x0;
    int y0;
    int x1;
    int y1;
    uint64_t reload_time;
    GLuint vao;
    program_t effect_program;
    program_t post_program;
    program_t bloom_pre_program;
    program_t bloom_x_program;
    program_t bloom_y_program;
    int programs_ok;
    GLuint noise_texture;
    fbo_t fbs[FBS];
    fbo_t quarter_fbs[QUARTER_FBS];
    size_t firstpass_fb_idx;
} demo_t;

static fbo_t create_framebuffer(GLsizei width, GLsizei height, GLint filter) {
    fbo_t fbo = {0};

    glGenFramebuffers(1, &fbo.framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo.framebuffer);
    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &fbo.texture);
    glBindTexture(GL_TEXTURE_2D, fbo.texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA,
                 GL_HALF_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           fbo.texture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        SDL_Log("FBO not complete\n");
        return (fbo_t){0};
    }

    fbo.width = width;
    fbo.height = height;

    return fbo;
}

static int replace_program(program_t *old, program_t new) {
    if (!new.handle) {
#ifndef DEBUG
        abort();
#endif
        return 0;
    }
    if (old->handle) {
        program_deinit(old);
    }
    *old = new;

    return 1;
}

void demo_reload(demo_t *demo) {
    demo->programs_ok = 1;

    shader_t vertex_shader = compile_shader(vertex_shader_src, "vert", NULL, 0);
    shader_t fragment_shader =
        compile_shader_file("shaders/shader.frag", NULL, 0);

    demo->programs_ok &= replace_program(
        &demo->effect_program,
        link_program((shader_t[]){vertex_shader, fragment_shader}, 2));

    shader_t post_shader = compile_shader_file("shaders/post.frag", NULL, 0);

    demo->programs_ok &=
        replace_program(&demo->post_program, link_program(
                                                 (shader_t[]){
                                                     vertex_shader,
                                                     post_shader,
                                                 },
                                                 2));

    shader_t bloom_pre_shader =
        compile_shader_file("shaders/bloom_pre.frag", NULL, 0);

    demo->programs_ok &=
        replace_program(&demo->bloom_pre_program, link_program(
                                                      (shader_t[]){
                                                          vertex_shader,
                                                          bloom_pre_shader,
                                                      },
                                                      2));

    shader_t bloom_x_shader =
        compile_shader_file("shaders/blur.frag",
                            (shader_define_t[]){(shader_define_t){
                                .name = "HORIZONTAL", .value = "1"}},
                            1);

    demo->programs_ok &=
        replace_program(&demo->bloom_x_program, link_program(
                                                    (shader_t[]){
                                                        vertex_shader,
                                                        bloom_x_shader,
                                                    },
                                                    2));

    shader_t bloom_y_shader = compile_shader_file("shaders/blur.frag", NULL, 0);

    demo->programs_ok &=
        replace_program(&demo->bloom_y_program, link_program(
                                                    (shader_t[]){
                                                        vertex_shader,
                                                        bloom_y_shader,
                                                    },
                                                    2));

    shader_deinit(&vertex_shader);
    shader_deinit(&fragment_shader);
    shader_deinit(&post_shader);
    shader_deinit(&bloom_pre_shader);
    shader_deinit(&bloom_x_shader);
    shader_deinit(&bloom_y_shader);
    demo->reload_time = SDL_GetTicks64();
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

    for (size_t i = 0; i < FBS; i++) {
        demo->fbs[i] = create_framebuffer(width, height, GL_NEAREST);
        if (demo->fbs[i].framebuffer == 0) {
            return NULL;
        }
    }
    for (size_t i = 0; i < QUARTER_FBS; i++) {
        demo->quarter_fbs[i] =
            create_framebuffer(width / 2, height / 2, GL_LINEAR);
        if (demo->quarter_fbs[i].framebuffer == 0) {
            return NULL;
        }
    }

    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &demo->noise_texture);
    glBindTexture(GL_TEXTURE_2D, demo->noise_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, NOISE_SIZE, NOISE_SIZE, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, NULL);
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

static void set_rocket_uniforms(const program_t *program,
                                struct sync_device *rocket, double rocket_row) {
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

static void render_pass(const demo_t *demo, const fbo_t *draw_fb,
                        const program_t *program, struct sync_device *rocket,
                        double rocket_row, const GLuint *textures,
                        const char **sampler_ufm_names, size_t n_textures) {

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_fb->framebuffer);
    glClear(GL_COLOR_BUFFER_BIT);
    glViewport(0, 0, draw_fb->width, draw_fb->height);
    glUseProgram(program->handle);
    set_rocket_uniforms(program, rocket, rocket_row);
    glUniform1f(glGetUniformLocation(program->handle, "u_RocketRow"),
                rocket_row);
    glUniform2f(glGetUniformLocation(program->handle, "u_Resolution"),
                draw_fb->width, draw_fb->height);
    glUniform1i(glGetUniformLocation(program->handle, "u_NoiseSize"),
                NOISE_SIZE);

    for (size_t i = 0; i < n_textures; i++) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, textures[i]);
        glUniform1i(glGetUniformLocation(program->handle, sampler_ufm_names[i]),
                    i);
    }

    glBindVertexArray(demo->vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

void demo_render(demo_t *demo, struct sync_device *rocket, double rocket_row) {
    static unsigned char noise[NOISE_SIZE * NOISE_SIZE * 4];
    const size_t cur_fb_idx = demo->firstpass_fb_idx;
    const size_t alt_fb_idx = cur_fb_idx ? 0 : 1;

#ifdef DEBUG
    // Early return if shaders are currently unusable
    if (!demo->programs_ok) {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glClearColor(0.3, 0., 0., 1.);
        glClear(GL_COLOR_BUFFER_BIT);
        return;
    }
#endif

    glClearColor(0., 0., 0., 1.);

    // MAKE SOME NOISE !!!! WOOO
    // ------------------------------------------------------------------------

    glActiveTexture(GL_TEXTURE0);
    for (GLsizei i = 0; i < NOISE_SIZE * NOISE_SIZE * 4; i++) {
        noise[i] = rand_xoshiro();
    }
    glBindTexture(GL_TEXTURE_2D, demo->noise_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, NOISE_SIZE, NOISE_SIZE, GL_RGBA,
                    GL_UNSIGNED_BYTE, noise);

    // Effect shader
    // ------------------------------------------------------------------------

    render_pass(demo, &demo->fbs[cur_fb_idx], &demo->effect_program, rocket,
                rocket_row,
                (GLuint[]){demo->fbs[alt_fb_idx].texture, demo->noise_texture},
                (const char *[]){"u_FeedbackSampler", "u_NoiseSampler"}, 2);

    // Bloom pre
    // ------------------------------------------------------------------------

    render_pass(demo, &demo->quarter_fbs[0], &demo->bloom_pre_program, rocket,
                rocket_row, (GLuint[]){demo->fbs[cur_fb_idx].texture},
                (const char *[]){"u_InputSampler"}, 1);

    // Bloom x
    // ------------------------------------------------------------------------

    render_pass(demo, &demo->quarter_fbs[1], &demo->bloom_x_program, rocket,
                rocket_row, (GLuint[]){demo->quarter_fbs[0].texture},
                (const char *[]){"u_InputSampler"}, 1);

    // Bloom y
    // ------------------------------------------------------------------------

    render_pass(demo, &demo->quarter_fbs[0], &demo->bloom_y_program, rocket,
                rocket_row, (GLuint[]){demo->quarter_fbs[1].texture},
                (const char *[]){"u_InputSampler"}, 1);

    // Post shader
    // ------------------------------------------------------------------------

    render_pass(
        demo, &demo->fbs[2], &demo->post_program, rocket, rocket_row,
        (GLuint[]){demo->fbs[cur_fb_idx].texture, demo->quarter_fbs[0].texture,
                   demo->noise_texture},
        (const char *[]){"u_InputSampler", "u_BloomSampler", "u_NoiseSampler"},
        3);

    // Output blit
    // ------------------------------------------------------------------------

    glBindFramebuffer(GL_READ_FRAMEBUFFER, demo->fbs[2].framebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT);
#ifdef DEBUG
    // Animate shader reload to show feedback in debug builds
    float a = fmin((SDL_GetTicks64() - demo->reload_time) / 100.f, 1.);
    const GLint x0 = demo->x0 * a, x1 = demo->x1 * a, y0 = demo->y0 * a,
                y1 = demo->y1 * a, w = demo->width, h = demo->height;
#else
    const GLint x0 = demo->x0, x1 = demo->x1, y0 = demo->y0, y1 = demo->y1,
                w = demo->width, h = demo->height;
#endif
    glBlitFramebuffer(0, 0, w, h, x0, y0, x1, y1, GL_COLOR_BUFFER_BIT,
                      GL_LINEAR);

    // Switch fb to keep render results in memory for feedback effects
    demo->firstpass_fb_idx = alt_fb_idx;
}

void demo_deinit(demo_t *demo) {
    if (demo) {
        free(demo);
    }
}
