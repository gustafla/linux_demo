#include "filesystem.h"
#include "stb_vorbis.c"
#include <SDL2/SDL.h>

typedef struct {
    stb_vorbis *vorbis;
    int channels;
    int at_end;
    unsigned call_time;
} playback_t;

typedef struct {
    SDL_AudioDeviceID audio_device;
    SDL_AudioSpec spec;
    playback_t playback;
} music_player_t;

static void callback(void *userdata, Uint8 *stream, int len) {
    playback_t *playback = (playback_t *)userdata;
    memset(stream, 0, len);
    int num_floats = len / sizeof(float);
    if (stb_vorbis_get_samples_float_interleaved(
            playback->vorbis, playback->channels, (float *)stream,
            num_floats) == 0) {
        playback->at_end = 1;
    }
    // Update precise time
    playback->call_time = SDL_GetTicks();
}

static stb_vorbis *open_vorbis(const char *filename) {
    unsigned int len;
    const unsigned char *data = filesystem_open(filename, &len);
    if (!data) {
        SDL_Log("File %s not found\n", filename);
        return NULL;
    }

    int error = VORBIS__no_error;
    stb_vorbis *vorbis = stb_vorbis_open_memory(data, len, &error, NULL);

    if (error != VORBIS__no_error) {
        vorbis = NULL;
        SDL_Log("Failed to parse vorbis file %s\n", filename);
    }

    return vorbis;
}

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

int player_at_end(music_player_t *player) { return player->playback.at_end; }

int player_is_playing(music_player_t *player) {
    return SDL_GetAudioDeviceStatus(player->audio_device) == SDL_AUDIO_PLAYING;
}

double player_get_time(music_player_t *player) {
    double time = 0;

    SDL_LockAudioDevice(player->audio_device);
    double sample = stb_vorbis_get_sample_offset(player->playback.vorbis);
    SDL_UnlockAudioDevice(player->audio_device);
    time = sample / player->spec.freq;

    // add precision
    if (player_is_playing(player) && !player_at_end(player)) {
        time += (SDL_GetTicks() - player->playback.call_time) / 1000.;
    }

    return time;
}

void player_pause(music_player_t *player, int flag) {
    player->playback.call_time = SDL_GetTicks();
    SDL_PauseAudioDevice(player->audio_device, flag);
}

void player_set_time(music_player_t *player, double time) {
    size_t sample = time * player->spec.freq;
    SDL_LockAudioDevice(player->audio_device);
    stb_vorbis_seek(player->playback.vorbis, sample);
    player->playback.at_end = 0;
    SDL_UnlockAudioDevice(player->audio_device);
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
