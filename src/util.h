#ifndef UTIL_H
#define UTIL_H

#include <GL/gl.h>
#include <stddef.h>

size_t read_file(const char *file_path, char **dst);
GLuint compile_shader(const char *shader_src, const char *shader_type);
GLuint compile_shader_file(const char *filename);
GLuint link_program(size_t count, GLuint *shaders);

#endif
