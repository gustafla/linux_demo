#include "shader.h"
#include "config.h"
#include "filesystem.h"
#include "uniforms.h"
#include <GL/gl.h>
#include <GL/glext.h>
#include <SDL2/SDL_log.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SRC_FRAGMENTS 16

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

    *dst = (char *)malloc(len);
    fseek(file, 0, SEEK_SET);

    size_t read = fread(*dst, sizeof(char), len, file);
    fclose(file);
    if (read != len)
        goto cleanup;

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

shader_t compile_shader(const char *shader_src, size_t shader_src_len,
                        const char *shader_type, const shader_define_t *defines,
                        size_t n_defs) {
    static const char *src[MAX_SRC_FRAGMENTS];
    static GLint src_lens[MAX_SRC_FRAGMENTS];

    // Set src_lens to all 1-bits because that will cause them to be negative.
    //
    // glShaderSource documentation:
    //
    // Each element in the length array may contain the length of the
    // corresponding string (the null character is not counted as part
    // of the string length) or a value less than 0 to indicate that
    // the string is null terminated.
    memset(src_lens, ~0, MAX_SRC_FRAGMENTS * sizeof(GLint));

    shader_t ret = {0};
    ret.uniforms = parse_uniforms(shader_src, &ret.uniform_count);
    ret.handle = glCreateShader(type_to_enum(shader_type));

    size_t i = 0;
    src[i++] = GLSL_VERSION;
    if (defines) {
        for (size_t j = 0; j < n_defs && i + 5 < MAX_SRC_FRAGMENTS; j++) {
            src[i++] = "#define ";
            src[i++] = defines[j].name;
            src[i++] = " ";
            src[i++] = defines[j].value;
            src[i++] = "\n";
        }
    }
    src_lens[i] = shader_src_len;
    src[i++] = shader_src;

    glShaderSource(ret.handle, i, src, src_lens);
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
        glDeleteShader(ret.handle);
        return (shader_t){0};
    }

    return ret;
}

shader_t compile_shader_file(const char *filename,
                             const shader_define_t *defines, size_t n_defs) {
    char *shader_src = NULL;
    size_t shader_src_len = read_file(filename, &shader_src);
    if (shader_src_len == 0) {
        return (shader_t){0};
    }

    // Find file extension
    const char *shader_type = filename, *ret;
    do {
        if ((ret = strchr(shader_type, '.'))) {
            shader_type = ret + 1;
        }
    } while (ret);

    shader_t shader = compile_shader(shader_src, shader_src_len, shader_type,
                                     defines, n_defs);
#ifdef DEBUG
    free(shader_src);
#endif

    if (shader.handle == 0) {
        SDL_Log("File: %s\n", filename);
    }

    return shader;
}

program_t link_program(shader_t *shaders, size_t count) {
    program_t ret = (program_t){0};
    ret.handle = glCreateProgram();

    for (size_t i = 0; i < count; i++) {
        const GLuint shader = shaders[i].handle;
        if (!shader) {
            glDeleteProgram(ret.handle);
            return (program_t){0};
        }
        glAttachShader(ret.handle, shader);
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
        glDeleteProgram(ret.handle);
        return (program_t){0};
    }

    // Join all uniforms. Duplicates are allowed.
    for (size_t i = 0; i < count; i++) {
        ret.uniform_count += shaders[i].uniform_count;
    }
    ret.uniforms = malloc(ret.uniform_count * sizeof(uniform_t));
    size_t offset = 0;
    for (size_t i = 0; i < count; i++) {
        shader_t *s = shaders + i;
        memcpy(ret.uniforms + offset, s->uniforms,
               s->uniform_count * sizeof(uniform_t));
        offset += s->uniform_count;
    }

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
