#include "uniforms.h"
#include "gl.h"

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

    for (GLuint i = 0; i < *count; i++) {
        uniform_t *ufm = ufms + i;
        glGetActiveUniform(program, i, UFM_NAME_MAX - 1, &ufm->name_len,
                           &icount, &ufm->type, ufm->name);
        glGetActiveUniformsiv(program, 1, &i, GL_UNIFORM_BLOCK_INDEX,
                              &ufm->block_index);
        glGetActiveUniformsiv(program, 1, &i, GL_UNIFORM_OFFSET, &ufm->offset);
    }

    return ufms;
}

// This returns an array of uniform blocks found in program.
// See header uniforms.h for definition of uniform_block_t.
uniform_block_t *get_uniform_blocks(GLuint program, size_t *count) {
    GLsizei icount = 0;
    glGetProgramiv(program, GL_ACTIVE_UNIFORM_BLOCKS, &icount);
    if (icount < 1) {
        return NULL;
    }

    *count = (size_t)icount;

    uniform_block_t *blks = calloc(*count, sizeof(uniform_block_t));

    for (GLuint i = 0; i < *count; i++) {
        uniform_block_t *blk = blks + i;
        glGetActiveUniformBlockiv(program, i, GL_UNIFORM_BLOCK_DATA_SIZE,
                                  &blk->size);
    }

    return blks;
}
