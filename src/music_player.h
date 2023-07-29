#ifndef MUSIC_PLAYER_H
#define MUSIC_PLAYER_H

typedef struct music_player_t_ music_player_t;

music_player_t *music_player_init(const char *filename);
void music_player_deinit(music_player_t *player);
int player_at_end(music_player_t *player);
int player_is_playing(music_player_t *player);
double player_get_time(music_player_t *player);
void player_pause(music_player_t *player, int flag);
void player_set_time(music_player_t *player, double time);

#endif
