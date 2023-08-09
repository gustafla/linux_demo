#ifndef UNIFORMS_H
#define UNIFORMS_H

#include <stdlib.h>

#define UFM_NAME_MAX 32

// Currently supported GLSL uniform types are listed in this enum
typedef enum {
    UFM_UNKNOWN = 0,
    UFM_FLOAT,
    UFM_VEC2,
    UFM_VEC3,
    UFM_VEC4,
    UFM_INT,
} uniform_type_t;

// This struct represents a GLSL uniform declaration
typedef struct {
    uniform_type_t type;
    char name[UFM_NAME_MAX];
    size_t name_len;
} uniform_t;

uniform_t *parse_uniforms(const char *shader_src, size_t *count);

#endif
