#include "menu/help.h"

#include <stdio.h>

#include "gfx/screen.h"
#include "menu/forms.h"
#include "platform/sys.h"
#include "xvga/sound.h"

#define MAX_HELP_KEYS 8

/* grid: the 8x8 helpgrid surface; 0xFF is its colour key (left alone). */
static const unsigned char grid[8][8] = {
	{ 0xff, 0, 0xff, 0, 0xff, 0, 0xff, 0 },
	{ 0, 0xff, 0, 0xff, 0, 0xff, 0, 0xff },
	{ 0xff, 0, 0xff, 0, 0xff, 0, 0xff, 0 },
	{ 0, 0xff, 0, 0xff, 0, 0xff, 0, 0xff },
	{ 0xff, 0, 0xff, 0, 0xff, 0, 0xff, 0 },
	{ 0, 0xff, 0, 0xff, 0, 0xff, 0, 0xff },
	{ 0xff, 0, 0xff, 0, 0xff, 0, 0xff, 0 },
	{ 0, 0xff, 0, 0xff, 0, 0xff, 0, 0xff },
};

static int help_keys[MAX_HELP_KEYS];
static int help_key_count;

void draw_grid(int which)
{
	for (int y = 0; y < SCREEN_H; y++) {
		for (int x = 0; x < SCREEN_W; x++) {
			unsigned char g = grid[y & 7][x & 7];
			if (g != 0xff)
				screen.pixels[which][y * SCREEN_W + x] = g;
		}
	}
}

void set_help_key(int vk)
{
	if (help_key_count < MAX_HELP_KEYS)
		help_keys[help_key_count++] = vk;
}

void clear_help_keys(void)
{
	help_key_count = 0;
}

static int check_help_keys(void)
{
	for (int i = 0; i < help_key_count; i++) {
		if (keyispressed(help_keys[i]))
			return 1;
	}
	return 0;
}

void play_help(const help_t *help, int save)
{
	int n = get_numberofcontrols_inform();
	int buffer = -1, stop = 0;
	if (n == 0) {
		fprintf(stderr, "play_help: empty form\n");
		return;
	}
	blt_scr_2_scr(0, save, 0, 0, SCREEN_W, SCREEN_H, 0);
	/* Wait until the key that asked for help is up, or it would end the help at once. */
	while (check_help_keys())
		sys_sleep(5);
	for (; help->control_id != -1 && !stop; help++) {
		draw_grid(0);
		if (help->control_id < 0 || help->control_id >= n) {
			fprintf(stderr, "play_help: illegal control %d\n", help->control_id);
			continue;
		}
		buffer = play_sound_buffer_1(help->text, 0);
		call_help_function(help->control_id);
		xvga_show();
		sys_input in = { 0 };
		while (sound_buffer_busy(buffer)) {
			sys_poll(&in);
			if (check_help_keys() || in.quit) {
				stop = 1;
				break;
			}
			xvga_show();
			sys_sleep(10);
		}
	}
	stop_sound_buffer(buffer);
	blt_scr_2_scr(save, 0, 0, 0, SCREEN_W, SCREEN_H, 0);
	xvga_show();
	while (check_help_keys())
		sys_sleep(5);
	draw_form(0);
}
