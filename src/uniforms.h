#ifndef UNIFORMS_H
#define UNIFORMS_H

#include "gl.h"
#include <stdlib.h>

#define UFM_NAME_MAX 32

typedef struct {
    GLenum type;
    GLboolean is_rocket;
    GLsizei name_len;
    GLsizei track_len;
    GLchar name[UFM_NAME_MAX];
    GLchar track[UFM_NAME_MAX];
} uniform_t;

uniform_t *get_uniforms(GLuint program, size_t *count);

#endif
