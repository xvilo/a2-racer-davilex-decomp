/* The menu's sound helpers (mysound.cpp). A handle is a sound buffer index or -1;
 * every call honours the sound option and volumes. */
#ifndef A2_MYSOUND_H
#define A2_MYSOUND_H

extern int mysnd_state; /* 1: no sound device */

void snd_stop(int *handle);
void snd_play(int *handle, const char *filename);
void snd_play_looping(int *handle, const char *filename);
/* Waits until the sound has finished (keeping the window responsive), then stops it. */
void snd_wait(int *handle);
int snd_fade_out(int *handle);
int snd_set_volume(int *handle, int volume);
int snd_get_volume(int *handle);
/* The volume for an option level 0..4, in hundredths of a dB. */
int snd_volume_for_level(int level);

#endif
