#include "shader.h"
#include "config.h"
#include "filesystem.h"
#include "gl.h"
#include "preprocessor.h"
#include "uniforms.h"
#include <SDL2/SDL_log.h>
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// This function maps shader file extensions like "vert" or "frag" to an enum
// defined in shader.h
static GLenum type_from_str(const char *shader_type) {
    if (strcmp("vert", shader_type) == 0) {
        return GL_VERTEX_SHADER;
    } else if (strcmp("frag", shader_type) == 0) {
        return GL_FRAGMENT_SHADER;
    }
    SDL_Log("Unrecognized shader type: %s\n", shader_type);
    return GL_INVALID_ENUM;
}

// This function preprocesses and compiles a shader.
// Inputs:
//    `src`:         The base shader source code (excluding #version directive)
//    `src_len`:     The length of the shader source
//    `shader_type`: The shader's file extension (like "vert" or "frag")
//    `defines`:     An array of shader_define_t:s (see shader.h)
//    `count_def`:   The count of items in `defines`
// In all cases, the function returns a `shader_t`. Check its `handle`-field
// for value 0. If `handle` is 0, compilation failed.
GLuint compile_shader(const char *src, size_t src_len, const char *shader_type,
                      const shader_define_t *defines, size_t count_def) {

    // Create empty shader object
    GLuint shader = glCreateShader(type_from_str(shader_type));

    // Preprocess include-directives and inject define-directives
    const char *processed_src =
        preprocess_glsl(src, src_len, "shaders", defines, count_def);

    // Load the sources "into" OpenGL driver
    glShaderSource(shader, 1, &processed_src, NULL);

    // Free the processed source code
    free((void *)processed_src);

    // Compile
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
