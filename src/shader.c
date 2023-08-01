#include "shader.h"
#include "filesystem.h"
#include "uniforms.h"
#include <GL/gl.h>
#include <GL/glext.h>
#include <SDL2/SDL_log.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef DEBUG
static size_t read_file(const char *filename, char **dst) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        goto cleanup;
    }

    fseek(file, 0, SEEK_END);
    size_t len = ftell(file);
    if (!len) {
        goto cleanup;
    }

    *dst = (char *)malloc(len + 1);
    fseek(file, 0, SEEK_SET);

    size_t read = fread(*dst, sizeof(char), len, file);
    fclose(file);
    if (read != len)
        goto cleanup;

    (*dst)[len] = '\0';

    return len;

cleanup:
    if (*dst) {
        free(*dst);
        *dst = NULL;
    }
    SDL_Log("Failed to read file %s\n", filename);
    return 0;
}
#else
static size_t read_file(const char *filename, char **dst) {
    unsigned int len = 0;
    const unsigned char *data = filesystem_open(filename, &len);
    if (!data) {
        SDL_Log("Cannot find file %s\n", filename);
        return 0;
    }

    *dst = (char *)data;
    return len;
}
#endif

static GLenum type_to_enum(const char *shader_type) {
    if (strcmp("vert", shader_type) == 0) {
        return GL_VERTEX_SHADER;
    } else if (strcmp("geom", shader_type) == 0) {
        return GL_GEOMETRY_SHADER;
    } else if (strcmp("frag", shader_type) == 0) {
        return GL_FRAGMENT_SHADER;
    }
    SDL_Log("Unrecognized shader type: %s\n", shader_type);
    return GL_INVALID_ENUM;
}

shader_t compile_shader(const char *shader_src, const char *shader_type) {
    shader_t ret = {0};
    ret.uniforms = parse_uniforms(shader_src, &ret.uniform_count);
    ret.handle = glCreateShader(type_to_enum(shader_type));
    glShaderSource(ret.handle, 1, &shader_src, NULL);
    glCompileShader(ret.handle);

    GLint status;
    glGetShaderiv(ret.handle, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE) {
        GLint log_len;
        glGetShaderiv(ret.handle, GL_INFO_LOG_LENGTH, &log_len);
        GLchar *log = malloc(sizeof(GLchar) * log_len);
        glGetShaderInfoLog(ret.handle, log_len, NULL, log);
        SDL_Log("Shader compilation failed:\n%s\n", log);
        free(log);
        free(ret.uniforms);
        return (shader_t){0};
    }

    return ret;
}

shader_t compile_shader_file(const char *filename) {
    char *shader_src = NULL;

    if (read_file(filename, &shader_src) == 0) {
        return (shader_t){0};
    }

    // Find file extension
    const char *shader_type = filename, *ret;
    do {
        if ((ret = strchr(shader_type, '.'))) {
            shader_type = ret + 1;
        }
    } while (ret);

    shader_t shader = compile_shader(shader_src, shader_type);
#ifdef DEBUG
    free(shader_src);
#endif

    if (shader.handle == 0) {
        SDL_Log("File: %s\n", filename);
    }

    return shader;
}

program_t link_program(shader_t *shaders, size_t count,
                       const char *uniform_filter) {
    program_t ret = (program_t){0};
    ret.handle = glCreateProgram();

    for (size_t i = 0; i < count; i++) {
        glAttachShader(ret.handle, shaders[i].handle);
    }

    glLinkProgram(ret.handle);

    GLint status;
    glGetProgramiv(ret.handle, GL_LINK_STATUS, &status);
    if (status == GL_FALSE) {
        GLint log_len;
        glGetProgramiv(ret.handle, GL_INFO_LOG_LENGTH, &log_len);
        GLchar *log = malloc(sizeof(GLchar) * log_len);
        glGetProgramInfoLog(ret.handle, log_len, NULL, log);
        SDL_Log("Program linking failed.\n%s\n", log);
        free(log);
        return (program_t){0};
    }

    // Join all uniforms. Duplicates are allowed.
    size_t worst_case_size = 0;
    for (size_t i = 0; i < count; i++) {
        worst_case_size += shaders[i].uniform_count;
    }
    ret.uniforms = malloc(worst_case_size * sizeof(uniform_t));

    // Inefficient, but seldom called
    for (size_t i = 0; i < count; i++) {
        shader_t *s = shaders + i;

        for (size_t j = 0; j < s->uniform_count; j++) {
            uniform_t *ufm = s->uniforms + j;

            // Skip filtered
            for (const char *k = uniform_filter; *k; k = strchr(k, 0) + 1) {
                if (strcmp(ufm->name, k) == 0) {
                    goto next;
                }
            }

            ret.uniforms[ret.uniform_count] = *ufm;
            ret.uniform_count++;
        next:;
        }
    }

    print_uniforms(ret.uniforms, ret.uniform_count);

    return ret;
}

void shader_deinit(shader_t *shader) {
    glDeleteShader(shader->handle);
    free(shader->uniforms);
}

void program_deinit(program_t *program) {
    glDeleteProgram(program->handle);
    free(program->uniforms);
}
