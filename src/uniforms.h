#ifndef UNIFORMS_H
#define UNIFORMS_H

#include <stdlib.h>

#define UFM_NAME_MAX 32

typedef enum {
    UFM_UNKNOWN = 0,
    UFM_FLOAT,
    UFM_VEC2,
    UFM_VEC3,
    UFM_VEC4,
    UFM_INT,
} uniform_type_t;

typedef struct {
    uniform_type_t type;
    char name[UFM_NAME_MAX];
    size_t name_len;
} uniform_t;

uniform_t *parse_uniforms(const char *shader_src, size_t *count);
void print_uniforms(uniform_t *uniforms, size_t count);

#endif
