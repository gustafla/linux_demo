#include "config.h"
#include "demo.h"
#include "music_player.h"
#include "rocket_io.h"
#include <SDL2/SDL.h>
#include <sync.h>

#define ROW_RATE ((BEATS_PER_MINUTE / 60.) * ROWS_PER_BEAT)

#ifdef DEBUG
static void set_row(void *d, int row) {
    music_player_t *player = (music_player_t *)d;
    player_set_time(player, row / ROW_RATE);
}

static void pause(void *d, int flag) {
    music_player_t *player = (music_player_t *)d;
    player_pause(player, flag);
}

static int is_playing(void *d) {
    music_player_t *player = (music_player_t *)d;
    return player_is_playing(player);
}

static struct sync_cb rocket_callbacks = {
    .pause = pause,
    .set_row = set_row,
    .is_playing = is_playing,
};
#endif

static int poll_events(demo_t *demo, struct sync_device *rocket) {
    static SDL_Event e;

    // Get SDL events, such as keyboard presses or quit-signals
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            return 0;
        } else if (e.type == SDL_KEYDOWN) {
            if (e.key.keysym.sym == SDLK_ESCAPE || e.key.keysym.sym == SDLK_q) {
                return 0;
            }
#ifdef DEBUG
            if (e.key.keysym.sym == SDLK_s) {
                sync_save_tracks(rocket);
                SDL_Log("Tracks saved.\n");
            }
            if (e.key.keysym.sym == SDLK_r) {
                demo_reload(demo);
                SDL_Log("Shaders reloaded.\n");
            }
#endif
        } else if (e.type == SDL_WINDOWEVENT) {
            if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                int w, h;
                SDL_Window *window = SDL_GetWindowFromID(e.window.windowID);
                SDL_GL_GetDrawableSize(window, &w, &h);
                demo_resize(demo, w, h);
            }
        }
    }

    return 1;
}

int main(void) {
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
    int w = WIDTH, h = HEIGHT;
    SDL_Window *window =
        SDL_CreateWindow("demo", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         w, h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) {
        SDL_Log("SDL2 failed to initialize a window: %s\n", SDL_GetError());
        return 1;
    }

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
    if (!player) {
        return 1;
    }

    // Initialize demo rendering
    demo_t *demo = demo_init(WIDTH, HEIGHT);
    if (!demo) {
        return 1;
    }

#ifndef DEBUG // Put window in fullscreen when building a non-debug build
    SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    SDL_ShowCursor(SDL_DISABLE);
#endif

    // Resize demo to fit the window we actually got
    SDL_GL_GetDrawableSize(window, &w, &h);
    demo_resize(demo, w, h);

    // Initialize rocket
    struct sync_device *rocket = sync_create_device("data/sync");
    if (!rocket) {
        SDL_Log("Rocket initialization failed\n");
        return 1;
    }

#ifdef DEBUG
    // Connect rocket
    SDL_Log("Connecting to Rocket editor...\n");
    while (sync_tcp_connect(rocket, "localhost", SYNC_DEFAULT_PORT)) {
        // Check exit events while waiting for Rocket connection
        if (!poll_events(demo, rocket)) {
            return 0;
        }
        SDL_Delay(200);
    }
#else
    // Set rocket io callback
    sync_set_io_cb(rocket, &rocket_iocb);
#endif

    // Here starts the demo's main loop
    player_pause(player, 0);
    while (poll_events(demo, rocket)) {
#ifndef DEBUG
        // Quit the demo when music ends
        if (player_at_end(player)) {
            break;
        }
#endif

        // Get time from music player
        double rocket_row = player_get_time(player) * ROW_RATE;

#ifdef DEBUG
        // Update rocket
        if (sync_update(rocket, (int)rocket_row, &rocket_callbacks,
                        (void *)player)) {
            SDL_Log("Rocket disconnected\n");
            break;
        }
#endif

        // Render. This does draw calls.
        demo_render(demo, rocket, rocket_row);

        // Swap the render result to window, so that it becomes visible
        SDL_GL_SwapWindow(window);
    }

#ifdef DEBUG
    sync_save_tracks(rocket);
    SDL_Log("Tracks saved.\n");
#endif

    demo_deinit(demo);
    music_player_deinit(player);
    SDL_Quit();
    return 0;
}
