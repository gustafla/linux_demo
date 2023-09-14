#include "shader.h"
#include "config.h"
#include "filesystem.h"
#include "gl.h"
#include "uniforms.h"
#include <SDL2/SDL_log.h>
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef DEBUG
// This read_file implementation completely reads a file from disk.
// Returns the number of bytes in the file, or 0 if the read failed.
// Changes *dst via pointer dereference. If successful *dst will point to the
// data, or sets it to NULL if unsuccessful.
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
// This read_file implementation gives access to a file in the executable
// via `filesystem_open` (filesystem.c).
// Returns the number of bytes in the file, or 0 if the read failed.
// Changes *dst via pointer dereference. If successful *dst will point to the
// data, or sets it to NULL if unsuccessful.
static size_t read_file(const char *filename, char **dst) {
    unsigned int len = 0;
    const unsigned char *data = filesystem_open(filename, &len);
    if (!data) {
        SDL_Log("Cannot find file %s\n", filename);
        *dst = NULL;
        return 0;
    }

    *dst = (char *)data;
    return len;
}
#endif

// This function maps shader file extensions like "vert" or "frag" to an enum
// defined in shader.h
static GLenum type_from_str(const char *shader_type) {
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

// This function preprocesses, drives uniform parsing and compiles a shader.
// Inputs:
//    `shader_src`: The base shader source code (excluding #version directive)
//    `shader_src_len`: The length in bytes of `shader_src`.
//    `shader_type`: The shader's file extension (like "vert" or "frag")
//    `defines`: An array of shader_define_t:s (see shader.h)
//    `n_defs`: The count of items in `defines`
// In all cases, the function returns a `shader_t`. Check its `handle`-field
// for value 0. If `handle` is 0, compilation failed.
GLuint compile_shader(const char *shader_src, size_t shader_src_len,
                      const char *shader_type, const shader_define_t *defines,
                      size_t n_defs) {
    static const char *src[MAX_SHADER_FRAGMENTS];
    static GLint src_lens[MAX_SHADER_FRAGMENTS];

    // Create empty shader object
    GLuint shader = glCreateShader(type_from_str(shader_type));

    // glShaderSource documentation:
    //
    // Each element in the length array may contain the length of the
    // corresponding string (the null character is not counted as part
    // of the string length) or a value less than 0 to indicate that
    // the string is null terminated.
    //
    // Because most of our src fragments are null-terminated, we set the whole
    // array of src_lens to -1 by default.
    for (size_t i = 0; i < MAX_SHADER_FRAGMENTS; i++) {
        src_lens[i] = -1;
    }

    // Iterate defines and inject #define directives right after an injected
    // #version -directive.
    size_t i = 0;
    src[i++] = GLSL_VERSION;
    if (defines) {
        for (size_t j = 0; j < n_defs; j++) {
            assert(i + 4 < MAX_SHADER_FRAGMENTS);
            src[i++] = "#define ";
            src[i++] = defines[j].name;
            src[i++] = " ";
            src[i++] = defines[j].value;
            src[i++] = "\n";
        }
    }
    assert(i < MAX_SHADER_FRAGMENTS);
    // Remember to add shader_src_len to the right slot in src_lens, as
    // shader_src is not null terminated (it is read from disk or executable)
    src_lens[i] = shader_src_len;
    src[i++] = shader_src;

    glShaderSource(shader, i, src, src_lens);
    glCompileShader(shader);

    // Check and report errors
    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE) {
        GLint log_len;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_len);
        GLchar *log = malloc(sizeof(GLchar) * log_len);
        glGetShaderInfoLog(shader, log_len, NULL, log);
        SDL_Log("Shader compilation failed:\n%s\n", log);
        free(log);
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

// This function is similar to the one above, but it loads a file by filename
// first, before running compile_shader.
GLuint compile_shader_file(const char *filename, const shader_define_t *defines,
                           size_t n_defs) {
    char *shader_src = NULL;
    size_t shader_src_len = read_file(filename, &shader_src);
    if (shader_src_len == 0) {
        return 0;
    }

    // Find file extension
    const char *shader_type = filename, *ret;
    do {
        if ((ret = strchr(shader_type, '.'))) {
            shader_type = ret + 1;
        }
    } while (ret);

    GLuint shader = compile_shader(shader_src, shader_src_len, shader_type,
                                   defines, n_defs);
#ifdef DEBUG
    free(shader_src);
#endif

    if (shader == 0) {
        SDL_Log("File: %s\n", filename);
    }

    return shader;
}

// This function "combines" shaders to a usable shader program.
// In all cases, the function returns a `program_t`. Check its `handle`-field
// for value 0. If `handle` is 0, compilation failed.
program_t link_program(GLuint *shaders, size_t count) {
    program_t ret = (program_t){0};
    ret.handle = glCreateProgram();

    for (size_t i = 0; i < count; i++) {
        if (!shaders[i]) {
            glDeleteProgram(ret.handle);
            return (program_t){0};
        }
        glAttachShader(ret.handle, shaders[i]);
    }

    glLinkProgram(ret.handle);

    // Check and report errors
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

    // Query OpenGL for uniforms in the successfully linked program
    ret.uniforms = get_uniforms(ret.handle, &ret.uniform_count);
    ret.blocks = get_uniform_blocks(ret.handle, &ret.block_count);

    // Generate one buffer per uniform block
    ret.block_buffers = calloc(ret.block_count, sizeof(GLuint));
    glGenBuffers(ret.block_count, ret.block_buffers);
    for (size_t i = 0; i < ret.block_count; i++) {
        glBindBuffer(GL_UNIFORM_BUFFER, ret.block_buffers[i]);
        glBufferData(GL_UNIFORM_BUFFER, ret.blocks[i].size, NULL,
                     GL_STATIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    return ret;
}

void shader_deinit(GLuint shader) { glDeleteShader(shader); }

void program_deinit(program_t *program) {
    glDeleteBuffers(program->block_count, program->block_buffers);
    glDeleteProgram(program->handle);
    free(program->uniforms);
    free(program->blocks);
    free(program->block_buffers);
}
