#ifndef PREPROCESSOR_H
#define PREPROCESSOR_H

// Why aren't we using stb_include.h?
// The library would be ideal for our use,
// but it doesn't support our binary-embedded filesystem hack.

#include "shader.h"

const char *preprocess_glsl(const char *src, size_t src_len, const char *path,
                            const shader_define_t *defines, size_t count_def);

#endif
