#include "config.h"
#include "filesystem.h"
#include "shader.h"
#include <SDL2/SDL_log.h>
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

// Return the beginning of next (second) line from src
static char *next_line(char *src) {
    if (src == NULL) {
        return NULL;
    }
    char *line = strchr(src, '\n');
    if (line) {
        return line + 1;
    }
    return NULL;
}

// Find the first #include directive from shader_src
// Parameters:
//    `src`:    The characters to search from, null-terminated.
//    `start`:  Directive's start offset will be stored to `start`.
//    `rest`:   Offset to next line after directive will be stored to `rest`.
//    `lineno`: Line number of the line following #include
//              will be stored to `lineno`.
// Returns a null-terminated string containing the #included filename if found,
// otherwise returns NULL. Caller is responsible for freeing returned string.
static char *find_include(char *src, size_t *start, size_t *rest,
                          uint32_t *lineno) {
    char *line, *next = src;
    uint32_t line_i = 0;

    while (line = next, next = next_line(line), line_i++, line != NULL) {
        // Compute current line length (includes control characters)
        size_t line_len = next ? (size_t)(next - line) : strlen(line);

        // If line starts with #line
        if (strncmp(line, "#line ", 6) == 0) {
            uint32_t new_lineno = strtoul(line + 6, NULL, 10);
            if (new_lineno == 0) {
                SDL_Log("Failed to parse #line -directive\n");
            } else {
                line_i = new_lineno - 1;
            }
            // If line starts with #include
        } else if (strncmp(line, "#include ", 9) == 0) {
            // Find first " on line
            const char *quot1 = memchr(line, '"', line_len);
            if (!quot1) {
                continue;
            }

            // Find second " on line
            const char *namestart = quot1 + 1;
            line_len -= namestart - line;
            const char *quot2 = memchr(namestart, '"', line_len);
            if (!quot2) {
                continue;
            }

            size_t name_len = quot2 - namestart;

            // Reject empty names
            if (name_len == 0) {
                continue;
            }

            // Copy filename to new buffer
            char *buf = calloc(name_len + 1, sizeof(char));
            if (buf) {
                memcpy(buf, namestart, name_len);
                // Write start and end offsets for caller
                *start = line - src;
                *rest = (namestart + line_len) - src;
                *lineno = line_i + 1;
            }
            return buf;
        }
    }

    return NULL;
}

// Replace #include lines in src with file contents
// Parameters:
//    `src`:  The source code buffer, null-terminated
//    `len`:  Length of the source code buffer in bytes (including nul)
//    `path`: Directory prefix to search for files to include
// Returns a new null-terminated string which the caller should free.
// Invalidates `src`, dereferencing `src` after calling this function is UB.
static char *process_includes(char *src, size_t len, const char *path) {
    char *filename;
    size_t start, rest;
    uint32_t lineno;

    while ((filename = find_include(src, &start, &rest, &lineno))) {
        char *fullpath = path_join(path, filename);

        // Read file
        char *include_src = NULL;
        size_t include_src_len = read_file(fullpath, &include_src);
        if (include_src == NULL) {
            SDL_Log("Warning: failed to read included file %s\n", filename);
        }

        // Add space for two line-directives
        size_t linedir1_len = strlen("#line 1\n");
        size_t linedir2_len = strlen("#line ") + (size_t)log10(lineno) + 2;

        // Allocate more buffer space for source code inclusion
        if (include_src_len > 0) {
            len += linedir1_len;
            len += include_src_len;
            len += linedir2_len;
            src = realloc(src, len);
        }

        // Move rest of the file after include-directive to correct position,
        // leaving a "hole" for inclusion, or overwriting the #include if
        // nothing will be included (due to empty files, failure to read etc.)
        const size_t move = strlen(src + rest) + 1;
        char *dst = src + start + include_src_len + linedir1_len + linedir2_len;
        memmove(dst, src + rest, move);

        // Insert #line 1
        memcpy(src + start, "#line 1\n", linedir1_len);

        // Insert the source to be included
        memcpy(src + start + linedir1_len, include_src, include_src_len);

        // Insert #line n, care must be taken as snprintf always writes a \0
        dst = src + start + linedir1_len + include_src_len;
        char tmp = src[start + linedir1_len + include_src_len + linedir2_len];
        snprintf(dst, linedir2_len + 1, "#line %i\n", lineno);
        src[start + linedir1_len + include_src_len + linedir2_len] = tmp;

        if (include_src) {
            free(include_src);
        }
        free((void *)fullpath);
        free((void *)filename);
    }

    return src;
}

// Generate a processed GLSL shader for glShaderSource
// Parameters:
//    `src`:       The source code buffer
//    `len`:       Length of the source code buffer in bytes
//    `path`:      Directory prefix to search for files to include
//    `defines`:   An array of shader_define_t:s (see shader.h)
//    `count_def`: The count of items in `defines`
// Returns a new null-terminated string which the caller should free.
const char *preprocess_glsl(const char *src, size_t src_len, const char *path,
                            const shader_define_t *defines, size_t count_def) {
    // Create output buffer with #version -directive initial line
    size_t len = strlen(GLSL_VERSION) + 1;
    char *s = malloc(len);
    memcpy(s, GLSL_VERSION, len);

    // Iterate defines and inject #define directives right after an injected
    // #version -directive.
    if (defines) {
        for (size_t j = 0; j < count_def; j++) {
            len += strlen("#define ") + strlen(defines[j].name) + strlen(" ") +
                   strlen(defines[j].value) + strlen("\n");
            s = realloc(s, len);
            strcat(s, "#define ");
            strcat(s, defines[j].name);
            strcat(s, " ");
            strcat(s, defines[j].value);
            strcat(s, "\n");
        }
    }

    // Copy base source to output buffer
    len += src_len + strlen("#line 1\n");
    s = realloc(s, len);
    strcat(s, "#line 1\n");
    strncat(s, src, src_len);

    return process_includes(s, len, path);
}
