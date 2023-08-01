#ifndef SHADER_H
#define SHADER_H

#include "uniforms.h"
#include <GL/gl.h>
#include <stddef.h>

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

shader_t compile_shader(const char *shader_src, const char *shader_type);
shader_t compile_shader_file(const char *filename);
program_t link_program(shader_t *shaders, size_t count,
                       const char *uniform_filter);
void shader_deinit(shader_t *shader);
void program_deinit(program_t *program);

#endif
