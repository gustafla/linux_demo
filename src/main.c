#include "demo.h"
#include "music_player.h"
#include <SDL2/SDL.h>

#define WIDTH 1920
#define HEIGHT 1080

int main(int argc, char **argv) {
    // Initialize SDL
    // This is required to get OpenGL and audio to work
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_Log("SDL2 failed to initialize: %s\n", SDL_GetError());
        return 1;
    }

    // Set OpenGL version to 3.3 core
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_CORE);

    // Set OpenGL default framebuffer to sRGB without depth buffer
    // We won't need a depth buffer for running GLSL shaders
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1);

    // Create a window
    // This is what the demo gets rendered to.
    SDL_Window *window = SDL_CreateWindow(
        "demo", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
#ifndef DEBUG // Put window in fullscreen when building a non-debug build
            | SDL_WINDOW_FULLSCREEN
#endif
    );
    if (!window) {
        SDL_Log("SDL2 failed to initialize a window: %s\n", SDL_GetError());
        return 1;
    }

    // Hide cursor when building a non-debug build
#ifndef DEBUG
    SDL_ShowCursor(SDL_DISABLE);
#endif

    // Get an OpenGL context
    // This is needed to connect the OpenGL driver to the window we just created
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        SDL_Log("SDL2 failed to create an OpenGL context: %s\n",
                SDL_GetError());
        return 1;
    }

    // Initialize music player
    music_player_t *player = music_player_init("data/music.ogg");

    // Initialize demo rendering
    demo_t *demo = demo_init();

    // Here starts the demo's main loop
    SDL_Event e;
    int running = 1;

    while (running) {
        // Get SDL events, such as keyboard presses or quit-signals
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = 0;
            } else if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_ESCAPE ||
                    e.key.keysym.sym == SDLK_q) {
                    running = 0;
                }
            } // else if (e.type == SDL_WINDOWEVENT) {
            //    if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
            //        demo_resize(e.window.data1, e.window.data2);
            //    }
            //}
        }

        // Render. This does draw calls.
        demo_render(demo);

        // Swap the render result to window, so that it becomes visible
        SDL_GL_SwapWindow(window);
    }

    demo_deinit(demo);
    music_player_deinit(player);
    SDL_Quit();
    return 0;
}
