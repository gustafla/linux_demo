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
    } else if (strcmp("frag", shader_type) == 0) {
        return GL_FRAGMENT_SHADER;
    }
    SDL_Log("Unrecognized shader type: %s\n", shader_type);
    return GL_INVALID_ENUM;
}

// This function finds the first #include directive (whole line) from shader_src
// Inputs:
//    `shader_src`:     The characters to search from.
//    `shader_src_len`: The length of `shader_src` in bytes.
//    `start`:          Directive's start offset relative to `shader_src` will
//                      be written to `start`.
//    `end`:            Directive's end offset (start of next line) relative to
//                      `shader_src` will be written to `end`.
// Returns a null-terminated string containing the #included filename if found,
// otherwise returns NULL.
const char *find_include(const char *shader_src, const size_t shader_src_len,
                         size_t *start, size_t *end) {
    static char filename[MAX_INCLUDE_NAME_LEN];

    // Allocate search scratch
    char *buf = calloc(shader_src_len + 1, sizeof(char));
    memcpy(buf, shader_src, shader_src_len);

    // Iterate line by line
    for (char *tok = strtok(buf, "\n"); tok; tok = strtok(NULL, "\n")) {
        // If line starts with #include
        if (strncmp(tok, "#include ", 9) == 0) {
            // Find first " on line
            char *quot1 = strchr(tok, '"');
            if (!quot1) {
                continue;
            }

            // Find second " on line
            char *namestart = quot1 + 1;
            char *quot2 = strchr(namestart, '"');
            if (!quot2) {
                continue;
            }

            // Check if fits in filename buffer
            size_t len = quot2 - namestart;
            if (len >= MAX_INCLUDE_NAME_LEN) {
                SDL_Log("Too long include filename: %s\n", tok);
                continue;
            }

            // Write start and end offsets for caller parser
            *start = tok - buf;
            char *line_end = strchr(quot2, 0);
            if (line_end) {
                *end = (line_end - buf) + 1;
            } else {
                *end = shader_src_len;
            }

            // Copy filename to static buffer
            memcpy(filename, namestart, len);
            filename[len] = 0;

            free(buf);
            return filename;
        }
    }

    free(buf);
    return NULL;
}

// This function preprocesses and compiles a shader.
// Inputs:
//    `shader_src`:     The base shader source code (excluding #version
//                      directive)
//    `shader_src_len`: The length in bytes of `shader_src`.
//    `shader_type`:    The shader's file extension (like "vert" or "frag")
//    `defines`:        An array of shader_define_t:s (see shader.h)
//    `n_defs`:         The count of items in `defines`
// In all cases, the function returns a `shader_t`. Check its `handle`-field
// for value 0. If `handle` is 0, compilation failed.
GLuint compile_shader(const char *shader_src, size_t shader_src_len,
                      const char *shader_type, const shader_define_t *defines,
                      size_t n_defs) {
    static const char *src[MAX_SHADER_FRAGMENTS];
    static GLint src_len[MAX_SHADER_FRAGMENTS];
    static int src_allocated[MAX_SHADER_FRAGMENTS];

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
        src_len[i] = -1;
    }

    // Because debug builds allocate from the heap to load files, we must keep
    // track, and free after sources are loaded.
    memset(src_allocated, 0, sizeof(int) * MAX_SHADER_FRAGMENTS);

    // Iterate defines and inject #define directives right after an injected
    // #version -directive.
    size_t frag_idx = 0;
    src[frag_idx++] = GLSL_VERSION;
    if (defines) {
        for (size_t j = 0; j < n_defs; j++) {
            assert(frag_idx + 4 < MAX_SHADER_FRAGMENTS);
            src[frag_idx++] = "#define ";
            src[frag_idx++] = defines[j].name;
            src[frag_idx++] = " ";
            src[frag_idx++] = defines[j].value;
            src[frag_idx++] = "\n";
        }
    }

    // Iterate shader until all includes processed
    size_t start = 0, end = 0, total = 0;
    const char *include_name = NULL;
    while (total < shader_src_len &&
           (include_name = find_include(
                shader_src + total, shader_src_len - total, &start, &end))) {

        // Found #include in shader
        // If there is content before directive, "paste" it here
        if (start) {
            assert(frag_idx < MAX_SHADER_FRAGMENTS);
            src_len[frag_idx] = start;
            src[frag_idx++] = shader_src + total;
        }

        // Read the file that was requested and "paste" it here
        char *include_src = NULL;
        // This hack hardcodes a shaders/ -directory prefix to all #includes
        static char fullpath[MAX_INCLUDE_NAME_LEN + 8];
        memcpy(fullpath, "shaders/", 8);
        // TODO improve this code by adding robust include path handling
        strncpy(fullpath + 8, include_name, MAX_INCLUDE_NAME_LEN);
        size_t include_src_len = read_file(fullpath, &include_src);
        if (include_src_len) {
            assert(frag_idx < MAX_SHADER_FRAGMENTS);
            src_len[frag_idx] = include_src_len;
#ifdef DEBUG
            src_allocated[frag_idx] = GL_TRUE;
#endif
            src[frag_idx++] = include_src;
        } else {
            SDL_Log("Warning: failed to read included file %s\n", include_name);
        }

        // Keep track of progress so that next iteration only searches the
        // remaining source code fragment.
        total += end;
    }

    // Finally, "paste" the remaining shader code here, it doesn't contain any
    // #include directives.
    if (total < shader_src_len) {
        assert(frag_idx < MAX_SHADER_FRAGMENTS);
        src_len[frag_idx] = shader_src_len - total;
        src[frag_idx++] = shader_src + total;
    }

    // Load the sources "into" OpenGL driver. Our burden is now over.
    glShaderSource(shader, frag_idx, src, src_len);

#ifdef DEBUG
    // Because debug builds allocate from the heap to load files, we must keep
    // track, and free after sources are loaded.
    for (size_t i = 0; i < MAX_SHADER_FRAGMENTS; i++) {
        if (src_allocated[i]) {
            free((void *)src[i]);
        }
    }
#endif

    // Compile.
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
