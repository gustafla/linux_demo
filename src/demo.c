#include "config.h"
#include "gl.h"
#include "rand.h"
#include "shader.h"
#include "uniforms.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sync.h>

// Allocate this many FBO:s to run render passes.
#define FBS 3
// Allocate this many FBO:s with 1/4th resolution (width/2, height/2).
#define QUARTER_FBS 2

// A constant vertex shader, which uses gl_VertexID to output
// a viewport-filling quad. No buffers or Input Assembly needed.
static const char *vertex_shader_src =
    "out vec2 FragCoord;\n"
    "void main() {\n"
    "    vec2 c = vec2(-1, 1);\n"
    "    vec4 coords[4] = vec4[4](c.xxyy, c.yxyy, c.xyyy, c.yyyy);\n"
    "    FragCoord = coords[gl_VertexID].xy;\n"
    "    gl_Position = coords[gl_VertexID];\n"
    "}\n";

// This struct bundles FBO resources and metadata
typedef struct {
    GLuint framebuffer;
    GLuint texture;
    GLsizei width;
    GLsizei height;
} fbo_t;

// This messy struct is the backbone of our renderer.
typedef struct {
    // Holds the "internal" aspect ratio, not window aspect ratio
    double aspect_ratio;
    // These integers hold final output window scaling information
    int x0;
    int y0;
    int x1;
    int y1;
    // This integer is used for animating reloads
    uint64_t reload_time;
    // OpenGL core requires that we use a VAO when issuing any drawcalls
    GLuint vao;
    // Shader programs for render passes. The stars of this show.
    program_t effect_program;
    program_t post_program;
    program_t bloom_pre_program;
    program_t bloom_x_program;
    program_t bloom_y_program;
    // If integer value is 0, there is a problem with the shaders
    int programs_ok;
    // A RGBA noise texture is used in rendering
    GLuint noise_texture;
    // Our FBOs used for rendering every frame
    fbo_t fbs[FBS];
    fbo_t quarter_fbs[QUARTER_FBS];
    // This integer is the index ([] number) of the FB which holds current
    // frame's "main" target FBO. 0 or 1. This FB gets the base rendered image
    // before any post processing etc, and the other (0 or 1) holds the previous
    // frame's first pass result for feedback effects.
    size_t firstpass_fb_idx;
} demo_t;

// Framebuffers/FBs/FBOs are sort of like "invisible images" that you can draw
// to, instead of drawing directly to the window. This lets us draw stuff but
// then process the image further in a new pass, by sampling its texture.
// This function creates FBs with a desired resolution and texture sampling
// filter.
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

// This function replaces *old with new, but only if new has a non-zero
// handle (meaning, it compiled and linked successfully).
// Return value is 1 if new program is fine to use, 0 otherwise.
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

// This function drives all shader loading. Gets called on initialization,
// and also from event handler (main.c) if R is pressed.
void demo_reload(demo_t *demo) {
    // First, assume programs are ok.
    demo->programs_ok = 1;

    GLuint vertex_shader = compile_shader(
        vertex_shader_src, strlen(vertex_shader_src), "vert", NULL, 0);
    GLuint fragment_shader =
        compile_shader_file("shaders/shader.frag", NULL, 0);

    // If replace_program returns 0, programs_ok get set to 0 regardless of it's
    // current value.
    demo->programs_ok &= replace_program(
        &demo->effect_program,
        link_program((GLuint[]){vertex_shader, fragment_shader}, 2));

    GLuint post_shader = compile_shader_file("shaders/post.frag", NULL, 0);

    // If replace_program returns 0, programs_ok get set to 0 regardless of it's
    // current value.
    demo->programs_ok &=
        replace_program(&demo->post_program, link_program(
                                                 (GLuint[]){
                                                     vertex_shader,
                                                     post_shader,
                                                 },
                                                 2));

    GLuint bloom_pre_shader =
        compile_shader_file("shaders/bloom_pre.frag", NULL, 0);

    // If replace_program returns 0, programs_ok get set to 0 regardless of it's
    // current value.
    demo->programs_ok &=
        replace_program(&demo->bloom_pre_program, link_program(
                                                      (GLuint[]){
                                                          vertex_shader,
                                                          bloom_pre_shader,
                                                      },
                                                      2));

    GLuint bloom_x_shader =
        compile_shader_file("shaders/blur.frag",
                            (shader_define_t[]){(shader_define_t){
                                .name = "HORIZONTAL", .value = "1"}},
                            1);

    // If replace_program returns 0, programs_ok get set to 0 regardless of it's
    // current value.
    demo->programs_ok &=
        replace_program(&demo->bloom_x_program, link_program(
                                                    (GLuint[]){
                                                        vertex_shader,
                                                        bloom_x_shader,
                                                    },
                                                    2));

    GLuint bloom_y_shader = compile_shader_file("shaders/blur.frag", NULL, 0);

    // If replace_program returns 0, programs_ok get set to 0 regardless of it's
    // current value.
    demo->programs_ok &=
        replace_program(&demo->bloom_y_program, link_program(
                                                    (GLuint[]){
                                                        vertex_shader,
                                                        bloom_y_shader,
                                                    },
                                                    2));

    // Cleanup shader objects because they have already been linked to programs
    shader_deinit(vertex_shader);
    shader_deinit(fragment_shader);
    shader_deinit(post_shader);
    shader_deinit(bloom_pre_shader);
    shader_deinit(bloom_x_shader);
    shader_deinit(bloom_y_shader);
    demo->reload_time = SDL_GetTicks64();
}

// This ugly function computes rectangle coordinates for scaling/letterboxing
// output from internal aspect ratio to actual window size.
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

// "demo_t's constructor" (if this were C++...)
demo_t *demo_init(int width, int height) {
    demo_t *demo = calloc(1, sizeof(demo_t));
    if (!demo) {
        return NULL;
    }

    demo->aspect_ratio = (double)width / (double)height;
    demo_resize(demo, width, height);

    // VAO is a must in GL 3.3 core. Don't think about it too much.
    glGenVertexArrays(1, &demo->vao);
    glBindVertexArray(demo->vao);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Load shaders
    demo_reload(demo);

    // Create FBs
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

    // Allocate noise texture
    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &demo->noise_texture);
    glBindTexture(GL_TEXTURE_2D, demo->noise_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, NOISE_SIZE, NOISE_SIZE, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    return demo;
}

// This function returns a corresponding rocket track name for an uniform.
// Argument `c` is a "component suffix" such as x, y, z or w.
//
// Example: rocket_track_suffix(ufm, 'x') -> "Cam:Pos.x"
static const char *rocket_track_suffix(uniform_t *ufm, char c) {
    static char trackname[UFM_NAME_MAX];

    memcpy(trackname, ufm->track, ufm->track_len + 1);

    trackname[ufm->track_len] = '.';
    trackname[ufm->track_len + 1] = c;
    trackname[ufm->track_len + 2] = 0;

    return trackname;
}

// I don't like C macros, but let's limit the use of this one to the following
// function `set_rocket_uniforms`.
#define GET_VALUE(track_name)                                                  \
    sync_get_val(sync_get_track(rocket, track_name), rocket_row)

// This iterates all uniforms in program, and calls the appropriate
// rocket functions and glUniform functions to glue them together.
static void set_rocket_uniforms(const program_t *program,
                                struct sync_device *rocket, double rocket_row) {
    for (size_t i = 0; i < program->uniform_count; i++) {
        uniform_t *ufm = program->uniforms + i;

        // Check for r_ -prefix
        if (ufm->name_len < 3 || ufm->name[0] != 'r' || ufm->name[1] != '_') {
            continue;
        }

        GLuint location = glGetUniformLocation(program->handle, ufm->name);
        // assert(location == i);

        switch (ufm->type) {
        case GL_FLOAT:
            glUniform1f(location, GET_VALUE(ufm->track));
            break;
        case GL_FLOAT_VEC2:
            glUniform2f(location, GET_VALUE(rocket_track_suffix(ufm, 'x')),
                        GET_VALUE(rocket_track_suffix(ufm, 'y')));
            break;
        case GL_FLOAT_VEC3:
            glUniform3f(location, GET_VALUE(rocket_track_suffix(ufm, 'x')),
                        GET_VALUE(rocket_track_suffix(ufm, 'y')),
                        GET_VALUE(rocket_track_suffix(ufm, 'z')));
            break;
        case GL_FLOAT_VEC4:
            glUniform4f(location, GET_VALUE(rocket_track_suffix(ufm, 'x')),
                        GET_VALUE(rocket_track_suffix(ufm, 'y')),
                        GET_VALUE(rocket_track_suffix(ufm, 'z')),
                        GET_VALUE(rocket_track_suffix(ufm, 'w')));
            break;
        case GL_INT:
        case GL_SAMPLER_2D:
            glUniform1i(location, (GLint)GET_VALUE(ufm->track));
            break;
        default:
            printf("Unsupported shader uniform type: %d.\n", ufm->type);
            printf("Go add support for it in " __FILE__
                   " (static void set_rocket_uniforms())\n");
        }
    }
}

// This messy function is the most important one here. It uses a shader program,
// rocket, input textures etc. to draw the shader to an output (`draw_fb`).
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

    // Bind textures for upcoming draw operation
    for (size_t i = 0; i < n_textures; i++) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, textures[i]);
        glUniform1i(glGetUniformLocation(program->handle, sampler_ufm_names[i]),
                    i);
    }

    // Draw a screen-filling quad with the shader!
    glBindVertexArray(demo->vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

// This gets called once per frame from main loop (main.c)
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
    // This stretches or squashes the post-processed image to the window in
    // correct aspect ratio (framebuffer 0).

    glBindFramebuffer(GL_READ_FRAMEBUFFER, demo->fbs[2].framebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT);
#ifdef DEBUG
    // Animate shader reload to show feedback in debug builds
    float a = fmin((SDL_GetTicks64() - demo->reload_time) / 100.f, 1.);
    const GLint x0 = demo->x0 * a, x1 = demo->x1 * a, y0 = demo->y0 * a,
                y1 = demo->y1 * a;
#else
    const GLint x0 = demo->x0, x1 = demo->x1, y0 = demo->y0, y1 = demo->y1;
#endif
    glBlitFramebuffer(0, 0, demo->fbs[2].width, demo->fbs[2].height, x0, y0, x1,
                      y1, GL_COLOR_BUFFER_BIT, GL_LINEAR);

    // Switch fb to keep render results in memory for feedback effects
    demo->firstpass_fb_idx = alt_fb_idx;
}

void demo_deinit(demo_t *demo) {
    if (demo) {
        free(demo);
    }
}
