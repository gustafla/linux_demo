// This file is used to get the correct OpenGL header on each platform, instead
// of duplicating these #includes everywhere.
#ifdef __MINGW64__
// Use glew on MinGW since windows doesn't have GLVND
#include <GL/glew.h>
#else
#ifdef GLES
#include <GLES3/gl32.h>
#else
// On linux, this is fine. Makefile defines GL_GLEXT_PROTOTYPES so that these
// includes actually get us the full API.
#include <GL/gl.h>
#include <GL/glext.h>
#endif
#endif
