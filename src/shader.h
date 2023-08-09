#ifndef SHADER_H
#define SHADER_H

#include "gl.h"
#include "uniforms.h"
#include <stddef.h>

// This structure represents a "#define NAME VALUE" pair to be injected to
// GLSL source code. It is useful to allow configuring the compilation of
// the same GLSL shader source in different ways. For example, by setting a
// #define HORIZONTAL 1 to `shaders/blur.frag`, we change it to blur the image
// along the X axis instead of default Y axis.
typedef struct {
    const char *name;
    const char *value;
} shader_define_t;

// This represents a yet-to-be-linked shader object.
typedef struct {
    GLuint handle;
    uniform_t *uniforms;
    size_t uniform_count;
} shader_t;

// This represents a fully usable shader program. Check that it has a
// nonzero handle-field, 0 represents that an error happened.
typedef struct {
    GLuint handle;
    uniform_t *uniforms;
    size_t uniform_count;
} program_t;

shader_t compile_shader(const char *shader_src, size_t shader_src_len,
                        const char *shader_type, const shader_define_t *defines,
                        size_t n_defs);
shader_t compile_shader_file(const char *filename,
                             const shader_define_t *defines, size_t n_defs);
program_t link_program(shader_t *shaders, size_t count);
void shader_deinit(shader_t *shader);
void program_deinit(program_t *program);

#endif
