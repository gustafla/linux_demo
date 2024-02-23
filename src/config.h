#ifndef CONFIG_H
#define CONFIG_H

// This header file is mostly referenced by main.c, demo.c and shader.c.
// Contains demo's configuration values such as resolution and music BPM.

#define WIDTH 1920
#define HEIGHT 720
#define RESOLUTION_SCALE 1
#define BEATS_PER_MINUTE 16
#define ROWS_PER_BEAT 32.

// A RGBA noise texture is generated for every frame. It's pixel count is this
// value squared. This value affects required CPU->GPU bandwidth per frame.
#define NOISE_SIZE (256 / 2)

// Max length of #include filename in shaders
#define MAX_INCLUDE_NAME_LEN 128

// GLSL_VERSION is prefixed to every shader, change it if you need some other
// version than specified here.
#ifdef GLES
#define GLSL_VERSION "#version 300 es\n"
#else
#define GLSL_VERSION "#version 330 core\n"
#endif

#endif
