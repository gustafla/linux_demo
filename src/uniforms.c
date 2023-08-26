#include "uniforms.h"
#include "gl.h"
#include <GL/gl.h>
#include <string.h>

// This function returns a corresponding rocket track name for an uniform.
// Example: rocket_track_name(track, ufm) -> "Cam:Pos"
static void rocket_track_name(char *track, GLsizei *track_len, uniform_t *ufm) {
    // Adjust for r_ -prefix
    const char *name = ufm->name + 2;
    *track_len = ufm->name_len - 2;

    memcpy(track, name, *track_len + 1);

    // Replace second underscore with colon (tab) when possible
    char *underscore = memchr(track, '_', *track_len);
    if (underscore) {
        *underscore = ':';
    }
    // TODO replace this with uniform block support
}

// This returns an array of uniforms found in program.
// See header uniforms.h for definition of uniform_t.
uniform_t *get_uniforms(GLuint program, size_t *count) {
    GLsizei icount = 0;
    glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &icount);
    if (icount < 1) {
        return NULL;
    }

    *count = (size_t)icount;

    uniform_t *ufms = calloc(*count, sizeof(uniform_t));

    for (size_t i = 0; i < *count; i++) {
        uniform_t *ufm = ufms + i;
        glGetActiveUniform(program, i, UFM_NAME_MAX - 1, &ufm->name_len,
                           &icount, &ufm->type, ufm->name);

        // Check for r_ -prefix
        if (ufm->name_len < 3 || ufm->name[0] != 'r' || ufm->name[1] != '_') {
            continue;
        }

        ufm->is_rocket = GL_TRUE;
        rocket_track_name(ufm->track, &ufm->track_len, ufm);
    }

    return ufms;
}
