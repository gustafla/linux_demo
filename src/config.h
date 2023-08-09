#ifndef CONFIG_H
#define CONFIG_H

// This header file is mostly referenced by main.c, demo.c and shader.c.
// Contains demo's configuration values such as resolution and music BPM.

#define WIDTH 1920
#define HEIGHT 720
#define BEATS_PER_MINUTE 120.0
#define ROWS_PER_BEAT 8.

// A RGBA noise texture is generated for every frame. It's pixel count is this
// value squared. This value affects required CPU->GPU bandwidth per frame.
#define NOISE_SIZE 256

// Maximum number of supported shader code "parts" or fragments that can
// be concatenated in a single call to compile_shader. Increase this value if
// the limit is hit in your usage. Every additional injected #define adds 5
// fragments, for example.
#define MAX_SHADER_FRAGMENTS 16

// GLSL_VERSION is prefixed to every shader, change it if you need some other
// version than specified here.
#define GLSL_VERSION "#version 330 core\n"

#endif
