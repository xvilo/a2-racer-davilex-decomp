#include "race/hud.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx/screen.h"
#include "platform/sys.h"
#include "race/driver.h"
#include "race/math3d.h"
#include "race/object3d.h"
#include "race/opponent.h"
#include "race/player.h"
#include "race/race.h"
#include "race/race_files.h"
#include "race/weg.h"
#include "res/archive.h"
#include "res/pcx.h"

int radar_on = 1;

/* anims in A2SPEL.DAT: the race's sprite animations, as GAMESPR image numbers. */
static const struct {
	int frame_count;
	const int *images;
} race_anims[35] = {
	/*  0 */ { 1, (const int[]){ 1 } },
	/*  1 */ { 40, (const int[]){ 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236 } },
	/*  2 */ { 20, (const int[]){ 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110 } },
	/*  3 */ { 20, (const int[]){ 249, 250, 251, 252, 253, 254, 255, 256, 257, 258, 259, 260, 261, 262, 263, 264, 265, 266, 267, 268 } },
	/*  4 */ { 15, (const int[]){ 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90 } },
	/*  5 */ { 40, (const int[]){ 4, 5, 6, 7, 8, 9, 10, 11, 13, 14, 15, 16, 17, 18, 19, 111, 112, 113, 114, 115, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176 } },
	/*  6 */ { 40, (const int[]){ 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216 } },
	/*  7 */ { 20, (const int[]){ 136, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156 } },
	/*  8 */ { 20, (const int[]){ 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196 } },
	/*  9 */ { 6, (const int[]){ 137, 243, 244, 245, 246, 247 } },
	/* 10 */ { 6, (const int[]){ 248, 237, 238, 239, 240, 241 } },
	/* 11 */ { 1, (const int[]){ 12 } },
	/* 12 */ { 1, (const int[]){ 242 } },
	/* 13 */ { 1, (const int[]){ 21 } },
	/* 14 */ { 10, (const int[]){ 31, 22, 23, 24, 25, 26, 27, 28, 29, 30 } },
	/* 15 */ { 1, (const int[]){ 42 } },
	/* 16 */ { 10, (const int[]){ 32, 33, 34, 35, 36, 37, 38, 39, 40, 41 } },
	/* 17 */ { 5, (const int[]){ 48, 49, 50, 51, 43 } },
	/* 18 */ { 5, (const int[]){ 52, 44, 45, 46, 47 } },
	/* 19 */ { 16, (const int[]){ 299, 285, 286, 287, 288, 289, 290, 291, 292, 293, 294, 295, 296, 297, 298, 284 } },
	/* 20 */ { 16, (const int[]){ 283, 269, 270, 271, 272, 273, 274, 275, 276, 277, 278, 279, 280, 281, 282, 53 } },
	/* 21 */ { 1, (const int[]){ 300 } },
	/* 22 */ { 1, (const int[]){ 302 } },
	/* 23 */ { 1, (const int[]){ 324 } },
	/* 24 */ { 1, (const int[]){ 323 } },
	/* 25 */ { 1, (const int[]){ 331 } },
	/* 26 */ { 1, (const int[]){ 332 } },
	/* 27 */ { 10, (const int[]){ 312, 313, 314, 315, 316, 317, 318, 319, 320, 303 } },
	/* 28 */ { 10, (const int[]){ 54, 301, 304, 305, 306, 307, 308, 309, 310, 311 } },
	/* 29 */ { 1, (const int[]){ 326 } },
	/* 30 */ { 1, (const int[]){ 325 } },
	/* 31 */ { 1, (const int[]){ 328 } },
	/* 32 */ { 1, (const int[]){ 329 } },
	/* 33 */ { 1, (const int[]){ 327 } },
	/* 34 */ { 1, (const int[]){ 330 } },
};

static sprite_archive gamespr;
static uint8_t dashboard_picture[SCREEN_W * SCREEN_H]; /* surscrn.pcx */
static uint8_t race_palette[768];

int init_hud(game_t *g)
{
	if (sprite_archive_open(&gamespr, g->data_dir, "GAMESPR")) {
		fprintf(stderr, "cannot read GAMESPR\n");
		return -1;
	}
	set_mem_images(&gamespr);
	/* The race's palette comes with the dashboard picture. */
	size_t size;
	const uint8_t *data = race_file("surscrn.pcx", &size);
	pcx_image img;
	if (data && !pcx_decode(data, size, &img)) {
		memcpy(race_palette, img.palette, 768);
		memset(dashboard_picture, 0xff, sizeof dashboard_picture);
		for (int y = 0; y < img.height && y < SCREEN_H; y++)
			memcpy(&dashboard_picture[y * SCREEN_W], &img.pixels[y * img.width],
				(size_t)(img.width < SCREEN_W ? img.width : SCREEN_W));
		pcx_free(&img);
	}
	return 0;
}

void exit_hud(void)
{
	set_mem_images(NULL);
	sprite_archive_close(&gamespr);
}

void hud_image(int anim, int frame, int x, int y, int flags)
{
	if (anim < 0 || anim >= 35 || frame < 0 || frame >= race_anims[anim].frame_count)
		return;
	set_mem_image(SCREEN_BACK, race_anims[anim].images[frame], x, y, flags);
}

void hud_begin(void)
{
	uint8_t *p = screen.pixels[SCREEN_BACK];
	if (dashboard_on_screen) {
		/* The dashboard below the 3D picture (which is 640x300 then). */
		memset(p, 0xff, SCREEN_W * 300);
		memcpy(p + SCREEN_W * 300, dashboard_picture + SCREEN_W * 300, SCREEN_W * 180);
	}
	else {
		memset(p, 0xff, SCREEN_W * SCREEN_H);
	}
	memcpy(screen.palette, race_palette, 768);
}

void hud_end(void)
{
	sys_gl_overlay(screen.pixels[SCREEN_BACK], race_palette, 0xff);
}

/* --- radar.cpp ------------------------------------------------------------------- */

void draw_radar(void)
{
	int rx, ry, w, h, big;
	if (!radar_on)
		return;
	if (dashboard_on_screen) {
		rx = 0x1e6;
		ry = 0x179;
		w = h = 0x3a;
		big = 0;
	}
	else {
		rx = 6;
		ry = 0x166;
		w = h = 0x74;
		big = 1;
	}
	int cx = w / 2, cy = w / 2;
	uint8_t *dst = screen.pixels[SCREEN_BACK];
#define RADAR_PUT(x, y, c) dst[(ry + (y)) * SCREEN_W + rx + (x)] = (uint8_t)(c)
	/* Background: without the dashboard every other pixel of every other row black (the
	 * rest see-through), with it all black. */
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			if (dashboard_on_screen)
				RADAR_PUT(x, y, 0);
			else if (y % 2 == 1 && x % 2 == 0)
				RADAR_PUT(x, y, 0);
		}
	}
	/* The player: two pixels wide, two (four) high. */
	int n = big ? 4 : 2;
	for (int y = 0; y < n; y++) {
		for (int x = 0; x < n; x++)
			RADAR_PUT(cx + x, cy + y, 0x14);
	}
	int a = -player_heading & ANGLE_MASK;
	float c = cos_table[a], s = sin_table[a];
	int chunk = opponent_data[objects3d[player_index].opponent].chunk - 10;
	for (int i = 0; i < 0x14; i++, chunk++) {
		if (chunk < 0 || chunk >= weg_chunk_count - 1)
			continue;
		for (int lane = 0; lane < 0xb; lane++) {
			int op = get_map_occupant(chunk, lane);
			if (op == -1 || op == 0)
				continue;
			const opponent_t *o = &opponent_data[op];
			float dx = (float)(int)(o->x - player_x) / 12.0f;
			float dz = (float)(int)(o->z - player_z) / 12.0f;
			int x = (int)((float)cx + (dx * c - dz * s));
			int y = (int)((float)h - ((float)cy + dz * c + dx * s));
			/* Civilians dark, rivals and police brighter. */
			int colour = o->field_e4 == DRIVER_RIVAL ? 0x3c : o->field_e4 == DRIVER_POLICE ? 0x58 : 0x0a;
			if (0 < x && x < w - 4 && 0 < y && y < h - 4) {
				for (int yy = 0; yy < n; yy++) {
					for (int xx = 0; xx < n; xx++)
						RADAR_PUT(x + xx, y + yy, colour);
				}
			}
		}
	}
#undef RADAR_PUT
}

/* --- dashbrd.cpp ----------------------------------------------------------------- */

static int speed_frame(void)
{
	if (fabsf(player_speed) == 0.0f)
		return 0;
	int f = abs((int)(player_speed * 2.666666666666666f));
	return f > 0x27 ? 0x27 : f;
}

static int fuel_frame(void)
{
	if (fuel == 0)
		return 0;
	int f = abs((int)(fuel * 0.06666666666666667f));
	return f > 0x13 ? 0x13 : f;
}

static void draw_race_position(void)
{
	const int *ranking = compute_race_ranking();
	int place = (ranking ? ranking[0] : 6) - 1;
	if (place < 0)
		place = 0;
	if (place > 5)
		place = 5;
	hud_image(10, place, 0x24e, 0x19, 0);
	hud_image(12, 0, 0x1fc, 0xf, 2);
}

static void draw_time_left(void)
{
	int h = time_left / 3600, m = time_left / 60 % 60, s = time_left % 60;
	int x = 0x42, y = 0x18;
	hud_image(0xd, 0, 0x10, 0x10, 2);
	hud_image(0xe, h / 10, x, y, 0);
	hud_image(0xe, h % 10, x + 0x14, y, 0);
	hud_image(0xe, m / 10, x + 0x39, y, 0);
	hud_image(0xe, m % 10, x + 0x4e, y, 0);
	hud_image(0xe, s / 10, x + 0x71, y, 0);
	hud_image(0xe, s % 10, x + 0x86, y, 0);
}

static void draw_money(void)
{
	int x = 0x42, y = 0x36, step = 0x14;
	hud_image(0x1d, 0, 0x10, 0x2e, 2);
	hud_image(0xe, money / 1000, x, y, 0);
	hud_image(0xe, money % 1000 / 100, x + step, y, 0);
	hud_image(0xe, money % 100 / 10, x + step * 2, y, 0);
	hud_image(0xe, money % 10, x + step * 3, y, 0);
}

void draw_dashboard(void)
{
	if (!dashboard_on_screen) {
		hud_image(6, speed_frame(), 0x1e4, 0x166, 2);
		hud_image(8, fuel_frame(), 0x1b0, 0x193, 2);
	}
	else {
		hud_image(0, 0, 0, 0x10b, 2);
		hud_image(4, player_steering_position() / 2, 0x2c, 0x15c, 2);
		hud_image(3, fuel_frame(), 0x124, 0x148, 2);
		hud_image(1, speed_frame(), 0xd7, 0x140, 2);
		int rev = engine_rpm == 0 ? 0 : abs((int)(engine_rpm * 0.2857142857142857f));
		hud_image(2, rev > 0x13 ? 0x13 : rev, 0x87, 0x13f, 2);
	}
	draw_race_position();
	draw_time_left();
	draw_money();
}
