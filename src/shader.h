#ifndef SHADER_H
#define SHADER_H

#include "uniforms.h"
#include <GL/gl.h>
#include <stddef.h>

typedef struct {
    const char *name;
    const char *value;
} shader_define_t;

typedef struct {
    GLuint handle;
    uniform_t *uniforms;
    size_t uniform_count;
} shader_t;

typedef struct {
    GLuint handle;
    uniform_t *uniforms;
    size_t uniform_count;
} program_t;

shader_t compile_shader(const char *shader_src, const char *shader_type,
                        const shader_define_t *defines, size_t n_defs);
shader_t compile_shader_file(const char *filename,
                             const shader_define_t *defines, size_t n_defs);
program_t link_program(shader_t *shaders, size_t count);
void shader_deinit(shader_t *shader);
void program_deinit(program_t *program);

#endif
