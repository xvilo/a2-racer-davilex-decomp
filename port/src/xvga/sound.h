/* Sound buffers (sound.cpp). The original kept up to max_sound_buffers DirectSound
 * buffers, each holding one WAV by name; playing a name reuses its buffer, or a
 * duplicate when it is still playing. The port keeps that bookkeeping and mixes the
 * buffers itself into an SDL audio stream. Volumes are DirectSound's: hundredths of a
 * decibel, -10000 (silent) to 0. */
#ifndef A2_SOUND_H
#define A2_SOUND_H

#include <stddef.h>
#include <stdint.h>

#include "res/archive.h"

/* DSBPLAY_LOOPING */
#define SOUND_LOOPING 1

/* Returns 0 on success (the original returned the sample rate it got, or an error). */
int xvga_init_sound(int max_buffers, const named_archive *files);
void xvga_exit_sound(void);

/* Loads `filename` from the archive if needed and plays it; returns the buffer index
 * or -1. */
int play_sound_buffer_1(const char *filename, int flags);
/* The same with a volume set before it starts. */
int play_sound_buffer_2(const char *filename, int volume, int flags);
/* A buffer for the file, not started; flags 4 keeps it loaded. */
int load_sound_buffer(const char *filename, int flags);
/* Starts a loaded buffer from the beginning. */
int play_sound_buffer(int index, int flags);
/* The playback rate in Hz (100..100000), DirectSound's SetFrequency. */
int set_sound_buffer_freq(int index, int freq);
/* Drops every buffer and sample and takes the sounds from another archive (the race
 * has its own process and sounds in the original). */
void xvga_set_sound_files(const named_archive *files, int max_buffers);
int sound_buffer_busy(int index);
int stop_sound_buffer(int index);
void stop_all_sound_buffers(void);
int set_sound_buffer_vol(int index, int volume);
int get_sound_buffer_vol(int index);
int max_sound_buffer_count(void);
/* Plays signed 16-bit PCM (copied); returns the buffer index or -1. */
int play_pcm(const int16_t *samples, size_t frames, int rate, int channels, int volume);

#endif
