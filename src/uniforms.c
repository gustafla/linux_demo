#include "uniforms.h"
#include <SDL2/SDL_log.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t min(size_t a, size_t b) { return (a < b) ? a : b; }

static char *my_strdup(const char *str) {
    size_t len = strlen(str);
    char *p = calloc(len + 1, sizeof(char));
    memcpy(p, str, len);
    return p;
}

static char *skip_spaces(char *tok) {
    while (*tok == ' ' || *tok == '\t') {
        tok++;
    }
    return tok;
}

static int match_str(const char *a, const char *b) {
    size_t len_a = strlen(a);
    size_t len_b = strlen(b);
    size_t len = min(len_a, len_b);
    if (!len) {
        return 0;
    }
    return memcmp(a, b, len) == 0;
}

static size_t parse_type(char *tok, uniform_type_t *type) {
    size_t type_len = 0;

    if (match_str(tok, "float")) {
        type_len = 5;
        *type = UFM_FLOAT;
    } else if (match_str(tok, "vec2")) {
        type_len = 4;
        *type = UFM_VEC2;
    } else if (match_str(tok, "vec3")) {
        type_len = 4;
        *type = UFM_VEC3;
    } else if (match_str(tok, "vec4")) {
        type_len = 4;
        *type = UFM_VEC4;
    } else if (match_str(tok, "int")) {
        type_len = 3;
        *type = UFM_INT;
    } else if (match_str(tok, "sampler2D")) {
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

uniform_t *parse_uniforms(const char *shader_src, size_t *count) {
    char *search = my_strdup(shader_src);

    *count = 0;
    uniform_t *results = malloc(sizeof(uniform_t));

    for (char *tok = strtok(search, ";\n"); tok; tok = strtok(NULL, ";\n")) {
        tok = skip_spaces(tok);

        if (match_str(tok, "uniform")) {
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
