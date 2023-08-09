#ifndef CONFIG_H
#define CONFIG_H

// This header file is mostly referenced by main.c and shader.c.
// Contains demo's configuration values such as resolution and music BPM.
// GLSL_VERSION is prefixed to every shader, change it if 330 core is not ok.

#define WIDTH 1920
#define HEIGHT 720
#define BEATS_PER_MINUTE 120.0
#define ROWS_PER_BEAT 8.
#define GLSL_VERSION "#version 330 core\n"

#endif
