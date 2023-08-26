#ifndef UNIFORMS_H
#define UNIFORMS_H

#include "gl.h"
#include <stdlib.h>

#define UFM_NAME_MAX 32

typedef struct {
    GLenum type;
    GLint block_index;
    GLint offset;
    GLsizei name_len;
    GLchar name[UFM_NAME_MAX];
} uniform_t;

typedef struct {
    GLint size;
} uniform_block_t;

uniform_t *get_uniforms(GLuint program, size_t *count);
uniform_block_t *get_uniform_blocks(GLuint program, size_t *count);

#endif
