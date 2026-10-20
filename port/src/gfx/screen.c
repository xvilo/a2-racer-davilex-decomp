#include "gfx/screen.h"

#include <string.h>

screen_t screen;

void screen_clear(int which, uint8_t color)
{
	memset(screen.pixels[which], color, sizeof screen.pixels[which]);
}

void screen_copy(int dst, int src)
{
	memcpy(screen.pixels[dst], screen.pixels[src], sizeof screen.pixels[dst]);
}

void screen_blit(int which, const uint8_t *pixels, int w, int h, int x, int y, int transparent)
{
	int sx = 0, sy = 0, cw = w, ch = h;
	if (x < 0) {
		sx = -x;
		cw += x;
		x = 0;
	}
	if (y < 0) {
		sy = -y;
		ch += y;
		y = 0;
	}
	if (x + cw > SCREEN_W)
		cw = SCREEN_W - x;
	if (y + ch > SCREEN_H)
		ch = SCREEN_H - y;
	if (cw <= 0 || ch <= 0)
		return;
	uint8_t *dst = screen.pixels[which] + y * SCREEN_W + x;
	const uint8_t *src = pixels + sy * w + sx;
	for (int row = 0; row < ch; row++, dst += SCREEN_W, src += w) {
		if (transparent < 0) {
			memcpy(dst, src, (size_t)cw);
			continue;
		}
		for (int i = 0; i < cw; i++) {
			if (src[i] != transparent)
				dst[i] = src[i];
		}
	}
}

void screen_blit_sprite(int which, const sprite *s, int x, int y)
{
	if (s && s->pixels)
		screen_blit(which, s->pixels, s->width, s->height, x, y, -1);
}
