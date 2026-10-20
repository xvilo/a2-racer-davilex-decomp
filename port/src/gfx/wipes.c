#include "gfx/wipes.h"

#include <stdio.h>

#include "gfx/screen.h"
#include "platform/sys.h"

/* The original drew each wipe step straight onto the visible surface and only called
 * Sleep(0) between steps, so the speed depended on the machine. The port shows every
 * step and waits this long, about what a 1997 PC took for a full-screen pass. */
#define WIPE_STEP_MS 25

/* raster: four 4x4 masks; pass s copies the pixels whose mask byte is set. Laid out as
 * in A2MENU.DAT: byte [s][row + col * 4]. */
static const unsigned char raster[4][16] = {
	{ 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1 },
	{ 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0 },
	{ 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0 },
	{ 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0 },
};

static void step_done(void)
{
	xvga_show();
	sys_sleep(WIPE_STEP_MS);
}

static void wipe_draw_pixel_grid(int src, int step, int x, int y, int w, int h)
{
	for (int by = y; by < y + h; by += 4) {
		for (int bx = x; bx < x + w; bx += 4) {
			for (int row = 0; row < 4 && by + row < SCREEN_H; row++) {
				int o = (by + row) * SCREEN_W + bx;
				for (int col = 0; col < 4; col++) {
					if (raster[step][row + col * 4])
						screen.pixels[SCREEN_FRONT][o + col] = screen.pixels[src][o + col];
				}
			}
		}
	}
}

static void wipe_pixelfade_full(int src, int x, int y, int w, int h)
{
	for (int i = 0; i < 4; i++) {
		wipe_draw_pixel_grid(src, i, x, y, w, h);
		step_done();
	}
}

/* Scrolls the picture in from the left edge, 8 pixels at a time. */
static void wipe_scroll_in_left(int src, int x, int y, int w, int h)
{
	for (int edge = x + w - 8; edge >= 0; edge -= 8) {
		int width = x + w - edge;
		for (int row = y; row < y + h; row++) {
			for (int i = 0; i < width && x + i < SCREEN_W; i++)
				screen.pixels[SCREEN_FRONT][row * SCREEN_W + x + i] =
					screen.pixels[src][row * SCREEN_W + edge + i];
		}
		step_done();
	}
}

void handle_wipe_1(int src, int type, int x, int y, int w, int h, int delay)
{
	switch (type) {
	case WIPE_PIXELFADE:
		wipe_pixelfade_full(src, x, y, w, h);
		break;
	case WIPE_SCROLL_LEFT:
		wipe_scroll_in_left(src, x, y, w, h);
		break;
	default:
		/* TODO: port the other wipes (wiperol.cpp, wipescrl.cpp, wipeluxa.cpp). */
		fprintf(stderr, "handle_wipe: type %d not ported, copying\n", type);
		blt_scr_2_scr(src, SCREEN_FRONT, x, y, x + w, y + h, 0);
		xvga_show();
		break;
	}
}

void handle_wipe(int src, int type, int delay)
{
	handle_wipe_1(src, type, 0, 0, SCREEN_W, SCREEN_H, delay);
}
