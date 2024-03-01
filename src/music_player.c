#include "filesystem.h"
#include "stb_vorbis.c"
#include <SDL2/SDL.h>

// This struct holds data needed in the SDL audio output thread's callback
typedef struct {
    // Points to the vorbis decoder/stream
    stb_vorbis *vorbis;
    // Number of channels
    int channels;
    // Flag for having reached the end
    int at_end;
} playback_t;

// This struct is for music player's main thread data
typedef struct {
    // SDL's audio device and specification we use
    SDL_AudioDeviceID audio_device;
    SDL_AudioSpec spec;
    // SDL_GetTicks value right after the player is paused/unpaused/seeked
    uint64_t start_offset_ticks;
    // Demo's time in seconds right after the player is paused/unpaused/seeked
    double start_offset_time;
    playback_t playback;
} music_player_t;

// This function gets repeatedly called from SDL when the audio device is
// set to be playing. Decodes audio samples to SDL's buffer (*stream).
static void callback(void *userdata, Uint8 *stream, int len) {
    playback_t *playback = (playback_t *)userdata;
    memset(stream, 0, len);
    int num_floats = len / sizeof(float);
    if (stb_vorbis_get_samples_float_interleaved(
            playback->vorbis, playback->channels, (float *)stream,
            num_floats) == 0) {
        // Set "at end" flag if vorbis_get_samples returns 0 (no samples)
        playback->at_end = 1;
    }
}

// This function opens stb vorbis by filename
static stb_vorbis *open_vorbis(const char *filename) {
    int error = VORBIS__no_error;

    stb_vorbis *vorbis = stb_vorbis_open_filename(filename, &error, NULL);

    if (error != VORBIS__no_error) {
        vorbis = NULL;
        SDL_Log("Failed to parse vorbis file %s\n", filename);
    }

    return vorbis;
}

// This function calls open_vorbis, opens an SDL audio device and sets up the
// playback struct and callback function pointer for SDL.
music_player_t *music_player_init(const char *filename) {
    music_player_t *player = calloc(1, sizeof(music_player_t));
    if (!player) {
        return NULL;
    }

    stb_vorbis *vorbis = open_vorbis(filename);
    if (!vorbis) {
        return NULL;
    }

    stb_vorbis_info info = stb_vorbis_get_info(vorbis);
    player->playback.vorbis = vorbis;
    player->playback.channels = info.channels;

    // this spec needed to play the file back
    SDL_AudioSpec desired = {.freq = info.sample_rate,
                             .format = AUDIO_F32SYS,
                             .channels = info.channels,
                             .samples = 4096,
                             .callback = callback,
                             .userdata = (void *)&player->playback};

    // prepare the audio hardware
    player->audio_device =
        SDL_OpenAudioDevice(NULL, 0, &desired, &player->spec, 0);
    if (!player->audio_device) {
        SDL_Log("Failed to open audio device:\n%s\n", SDL_GetError());
        return NULL;
    }

    return player;
}

// Returns 1 if at end, 0 otherwise.
int player_at_end(music_player_t *player) { return player->playback.at_end; }

// Returns 1 if audio is playing, 0 otherwise.
int player_is_playing(music_player_t *player) {
    return SDL_GetAudioDeviceStatus(player->audio_device) == SDL_AUDIO_PLAYING;
}

// Returns current music playback time in seconds
double player_get_time(music_player_t *player) {
    double elapsed = 0;
    if (player_is_playing(player)) {
        uint64_t ticks_since = SDL_GetTicks64() - player->start_offset_ticks;
        elapsed = (double)ticks_since / 1000.0;
    }
    return elapsed + player->start_offset_time;
}

// Seeks the player to a time in seconds.
void player_set_time(music_player_t *player, double time) {
    size_t sample = time * player->spec.freq;
    SDL_LockAudioDevice(player->audio_device);
    stb_vorbis_seek(player->playback.vorbis, sample);
    player->playback.at_end = 0;
    SDL_UnlockAudioDevice(player->audio_device);
    player->start_offset_time = time;
    player->start_offset_ticks = SDL_GetTicks64();
}

// This function pauses/unpauses (flag) the player
void player_pause(music_player_t *player, int flag) {
    player_set_time(player, player_get_time(player));
    SDL_PauseAudioDevice(player->audio_device, flag);
}

void music_player_deinit(music_player_t *player) {
    if (player) {
        if (player->audio_device) {
            SDL_CloseAudioDevice(player->audio_device);
        }
        if (player->playback.vorbis) {
            stb_vorbis_close(player->playback.vorbis);
        }
        free(player);
    }
}
