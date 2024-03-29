#include "config.h"
#include "demo.h"
#include "gl.h"
#include "music_player.h"
#include <SDL2/SDL.h>
#include <sync.h>

// The surrounding () parentheses are actually important!
// Without them, the expression could be changed by it's surroundings
// after the macro is "inlined" in the preprocessor.
#define ROW_RATE ((BEATS_PER_MINUTE / 60.) * ROWS_PER_BEAT)

// The following functions and sync_cb struct are used to glue rocket to
// our music player.
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

// This handles SDL2 events. Returns 1 to "keep running" or 0 to "stop"/exit.
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

// Connects rocket while keeping SDL events polled. Returns 1 when successful,
// 0 when unsuccessful.
#ifdef DEBUG
static int connect_rocket(struct sync_device *rocket, demo_t *demo) {
    SDL_Log("Connecting to Rocket editor...\n");
    while (sync_tcp_connect(rocket, "localhost", SYNC_DEFAULT_PORT)) {
        // Check exit events while waiting for Rocket connection
        if (!poll_events(demo, rocket)) {
            return 0;
        }
        SDL_Delay(200);
    }
    SDL_Log("Connected.\n");
    return 1;
}
#endif

int main(int argc, char *argv[]) {
    // Initialize SDL
    // This is required to get OpenGL and audio to work
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_Log("SDL2 failed to initialize: %s\n", SDL_GetError());
        return 1;
    }

    // Set OpenGL version (ES 3.1 when GLES configured in make)
#ifdef GLES
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#else
    // Set OpenGL version (3.3 core is the default)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_CORE);
#endif

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

#ifdef __MINGW64__
    // On windows, we need to actually load/"wrangle" some OpenGL functions
    // at runtime. We use glew for that, because it's a hassle without using
    // a library.
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        SDL_Log("glew initialization failed.\n %s\n", glewGetErrorString(err));
        return 1;
    }
#endif

    // Initialize music player
    music_player_t *player = music_player_init("data/music.ogg");
    if (!player) {
        return 1;
    }

    // Initialize demo rendering
    demo_t *demo =
        demo_init(WIDTH * RESOLUTION_SCALE, HEIGHT * RESOLUTION_SCALE);
    if (!demo) {
        return 1;
    }

#ifndef DEBUG
    // Put window in fullscreen when building a non-debug build
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
    if (!connect_rocket(rocket, demo)) {
        return 0;
    }

    // Set up framerate counting
    uint64_t frames = 0;
    uint64_t frame_check_time = SDL_GetTicks64();
    uint64_t max_frame_time = 0;
    uint64_t timestamp = 0;
#endif

    // Here starts the demo's main loop
    player_pause(player, 0);
    while (poll_events(demo, rocket)) {
        // Get time from music player
        double time = player_get_time(player);
        double rocket_row = time * ROW_RATE;

#ifdef DEBUG
        // Update rocket in a loop until timed out or row changes.
        // This can cause a net delay up to n * m milliseconds, where
        // n = 10 (hardcoded loop) and m = 20 (hardcoded SDL_Delay call).
        // Total 200 ms may be blocked to save power when nothing is changing
        // in the editor.
        for (int i = 0; i < 10; i++) {
            if (sync_update(rocket, (int)rocket_row, &rocket_callbacks,
                            (void *)player)) {
                SDL_Log("Rocket disconnected\n");
                if (!connect_rocket(rocket, demo)) {
                    return 0; // how many layers of nesting are you on
                    // like, maybe 5, or 6 right now. my dude
                }
            }
            // After previous update, if player time has changed (due to seek
            // or unpause), don't continue delaying, go render.
            if (player_is_playing(player) || player_get_time(player) != time) {
                break;
            }
            // Delay
            SDL_Delay(20);
        }

        // Print FPS reading every so often
        uint64_t ct = SDL_GetTicks64();
        uint64_t ft = ct - timestamp;
        max_frame_time = ft > max_frame_time ? ft : max_frame_time;
        timestamp = ct;
        if (frame_check_time + 5000 <= ct) {
            SDL_Log("FPS: %.1f, max frametime: %lu ms\n",
                    frames * 1000. / (double)(ct - frame_check_time),
                    max_frame_time);
            frames = 0;
            max_frame_time = 0;
            frame_check_time = ct;
        }
        frames++;
#else
        // Quit the demo when music ends
        if (player_at_end(player)) {
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
