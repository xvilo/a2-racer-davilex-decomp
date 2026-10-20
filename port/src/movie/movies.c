/* Movies (A2MOVIE.DAT, smacker.cpp): the intro, the end-of-game and the exit film.
 * The original switched to 320x200 (MOVIE.INI's default) or stretched the picture to
 * 640x480; the port always stretches it over the 640x480 screen. Escape, Enter, Space
 * or a mouse click stops it (init_smacker's keys). */
#include "movie/movies.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx/screen.h"
#include "platform/sys.h"
#include "res/archive.h"
#include "res/smacker.h"
#include "xvga/sound.h"

static int movie_key_pressed(const sys_input *in)
{
	return in->quit || in->mouse_clicked || in->key == KEY_ESCAPE || in->key == KEY_ENTER ||
		in->key == KEY_SPACE;
}

/* The decoded frame, stretched to the whole screen (nearest pixel). */
static void show_frame(const smacker *s)
{
	int w = smk_width(s), h = smk_height(s);
	const uint8_t *src = smk_pixels(s);
	uint8_t *dst = screen.pixels[SCREEN_FRONT];
	for (int y = 0; y < SCREEN_H; y++) {
		const uint8_t *row = src + (y * h / SCREEN_H) * w;
		for (int x = 0; x < SCREEN_W; x++)
			dst[y * SCREEN_W + x] = row[x * w / SCREEN_W];
	}
	memcpy(screen.palette, smk_palette(s), sizeof screen.palette);
	xvga_show();
}

int play_movie(const char *data_dir, const char *name)
{
	size_t size;
	uint8_t *data = read_file_ci(data_dir, name, &size);
	if (!data) {
		fprintf(stderr, "movie: %s not found\n", name);
		return -1;
	}
	smacker *s = smk_open(data, size);
	if (!s) {
		fprintf(stderr, "movie: %s is not a Smacker file\n", name);
		free(data);
		return -1;
	}
	int16_t *pcm = NULL;
	int rate = 0, channels = 0;
	size_t frames = smk_decode_audio(s, &pcm, &rate, &channels);
	int snd = frames ? play_pcm(pcm, frames, rate, channels, 0) : -1;
	free(pcm);

	uint32_t start = sys_ticks();
	sys_input in = { 0 };
	int stopped = 0;
	for (int f = 0; f < smk_frame_count(s) && !stopped; f++) {
		if (smk_next_frame(s))
			break;
		show_frame(s);
		/* SmackWait: hold each frame until its time has come. */
		uint32_t due = start + (uint32_t)((f + 1) * smk_frame_ms(s));
		do {
			sys_poll(&in);
			if (movie_key_pressed(&in)) {
				stopped = 1;
				break;
			}
			sys_sleep(2);
		} while ((int32_t)(sys_ticks() - due) < 0);
	}
	if (snd >= 0)
		stop_sound_buffer(snd);
	clear_screen(SCREEN_FRONT, 0);
	xvga_show();
	smk_close(s);
	free(data);
	return stopped;
}
