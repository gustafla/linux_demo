#define _POSIX_C_SOURCE 200809L

#include "uniforms.h"
#include <SDL2/SDL_log.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Evaluates to 1 if str fully contains prefix at the beginning, otherwise 0
#define STARTS_WITH(str, prefix) (strncmp(str, prefix, strlen(prefix)) == 0)

// This function returns the string `tok` without leading blank characters.
static char *skip_spaces(char *tok) {
    while (isblank(*tok)) {
        tok++;
    }
    return tok;
}

// Checks string `tok` if it contains a supported GLSL type. Returns the length
// of such type (in source code characters) when a type is found, and sets *type
// to match.
static size_t parse_type(char *tok, uniform_type_t *type) {
    size_t type_len = 0;

    if (STARTS_WITH(tok, "float")) {
        type_len = 5;
        *type = UFM_FLOAT;
    } else if (STARTS_WITH(tok, "vec2")) {
        type_len = 4;
        *type = UFM_VEC2;
    } else if (STARTS_WITH(tok, "vec3")) {
        type_len = 4;
        *type = UFM_VEC3;
    } else if (STARTS_WITH(tok, "vec4")) {
        type_len = 4;
        *type = UFM_VEC4;
    } else if (STARTS_WITH(tok, "int")) {
        type_len = 3;
        *type = UFM_INT;
    } else if (STARTS_WITH(tok, "sampler2D")) {
        type_len = 9;
        *type = UFM_INT;
    }

    // If type doesn't end in space, it's not valid
    if (!isblank(tok[type_len])) {
        *type = UFM_UNKNOWN;
        return 0;
    }

    return type_len;
}

// Checks string `tok` if it contains a usable GLSL identifier.
// Returns the length of such name if it is usable, and copies it to *name.
static size_t parse_name(const char *tok, char *name) {
    size_t len = strlen(tok);

    for (const char *i = tok; *i; i++) {
        // Reject other characters than alphanumeric, spaces or underscores
        if (!isalnum(*i) && !isblank(*i) && *i != '_') {
            return 0;
        }
    }

    // Reject if all spaces aren't at the end
    size_t spaces = 0;
    for (const char *i = tok; *i; i++) {
        if (isblank(*i)) {
            spaces++;
            continue;
        }

        if (spaces) {
            return 0;
        }
    }

    // Reject if only whitespace
    if (spaces == len) {
        return 0;
    }

    size_t final_len = len - spaces;
    if (final_len >= UFM_NAME_MAX) {
        SDL_Log("Uniform name too long. Maximum is %d\n", UFM_NAME_MAX - 1);
        return 0;
    }

    memcpy(name, tok, final_len);
    name[final_len] = 0;

    return final_len;
}

// Checks every line (\n) and every source code line (;) in shader_src and
// returns an array of uniforms that were found in it.
// Limitations: does not validate code, just ad hoc parses it.
// Does not support structs. Does not support uniforms that span multiple lines
// for example:
//
// uniform float
// thisIsNotSupported;
uniform_t *parse_uniforms(const char *shader_src, size_t *count) {
    char *search = strdup(shader_src);

    *count = 0;
    uniform_t *results = malloc(sizeof(uniform_t));

    for (char *tok = strtok(search, ";\n"); tok; tok = strtok(NULL, ";\n")) {
        tok = skip_spaces(tok);

        if (STARTS_WITH(tok, "uniform")) {
            uniform_t result = (uniform_t){0};

            tok += 7;
            tok = skip_spaces(tok);

            // Parse uniform type
            size_t type_len = parse_type(tok, &result.type);
            if (result.type == UFM_UNKNOWN) {
                SDL_Log("Unknown uniform type: %s!\n", tok);
                continue;
            }
            tok += type_len;

            // Parse uniform name
            tok = skip_spaces(tok);
            result.name_len = parse_name(tok, result.name);
            if (result.name_len == 0) {
                SDL_Log("Invalid name %s!\n", tok);
                continue;
            }

            // Store in results
            *count = *count + 1;
            results = realloc(results, *count * sizeof(uniform_t));
            results[*count - 1] = result;
        }
    }

    free(search);

    return results;
}
