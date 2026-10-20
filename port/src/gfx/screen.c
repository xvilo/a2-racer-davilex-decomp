#include "gfx/screen.h"

#include <string.h>

#include "platform/sys.h"

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

static const sprite_archive *mem_images;
static int clip_tx = 0, clip_ty = 0, clip_bx = SCREEN_W - 1, clip_by = SCREEN_H - 1;

void set_mem_images(const sprite_archive *images)
{
	mem_images = images;
}

/* Mem image numbers start at 1: image n is sprite n - 1 of the archive (a button's
 * pressed frame and the first stage's map only line up that way). */
const sprite *get_mem_image(int index)
{
	if (!mem_images || index < 1 || index > mem_images->count)
		return NULL;
	return &mem_images->sprites[index - 1];
}

void set_clip_limit(int x1, int y1, int x2, int y2)
{
	clip_tx = x1;
	clip_ty = y1;
	clip_bx = x2;
	clip_by = y2;
}

void set_mem_image(int which, int image, int x, int y, int flags)
{
	const sprite *s = get_mem_image(image);
	if (!s || !s->pixels)
		return;
	if (!(flags & MI_TOPLEFT)) {
		x -= s->center_x;
		y -= s->center_y;
	}
	int sx = 0, sy = 0, w = s->width, h = s->height;
	int tx = flags & MI_CLIP ? clip_tx : 0, ty = flags & MI_CLIP ? clip_ty : 0;
	int bx = flags & MI_CLIP ? clip_bx : SCREEN_W - 1, by = flags & MI_CLIP ? clip_by : SCREEN_H - 1;
	if (x < tx) {
		sx = tx - x;
		w -= sx;
		x = tx;
	}
	if (y < ty) {
		sy = ty - y;
		h -= sy;
		y = ty;
	}
	if (x + w > bx + 1)
		w = bx + 1 - x;
	if (y + h > by + 1)
		h = by + 1 - y;
	if (w <= 0 || h <= 0)
		return;
	for (int row = 0; row < h; row++) {
		const uint8_t *src = s->pixels + (sy + row) * s->width + sx;
		uint8_t *dst = screen.pixels[which] + (y + row) * SCREEN_W + x;
		for (int i = 0; i < w; i++) {
			if (src[i] != MEM_IMAGE_TRANSPARENT)
				dst[i] = src[i];
		}
	}
}

void blt_scr_2_scr(int src, int dst, int x1, int y1, int x2, int y2, int flags)
{
	if (x2 < 0 || x1 > SCREEN_W || y2 < 0 || y1 > SCREEN_H || src == dst)
		return;
	if (x1 < 0)
		x1 = 0;
	if (y1 < 0)
		y1 = 0;
	if (x2 > SCREEN_W)
		x2 = SCREEN_W;
	if (y2 > SCREEN_H)
		y2 = SCREEN_H;
	for (int y = y1; y < y2; y++)
		memcpy(screen.pixels[dst] + y * SCREEN_W + x1, screen.pixels[src] + y * SCREEN_W + x1,
			(size_t)(x2 - x1));
}

void clear_screen(int which, uint8_t color)
{
	screen_clear(which, color);
}

void xvga_show(void)
{
	sys_present(screen.pixels[SCREEN_FRONT], screen.palette);
}

uint8_t nearest_color(uint32_t colorref)
{
	int r = colorref & 0xFF, g = colorref >> 8 & 0xFF, b = colorref >> 16 & 0xFF;
	int best = 0, best_d = 1 << 30;
	for (int i = 0; i < 256; i++) {
		int dr = screen.palette[i * 3] - r, dg = screen.palette[i * 3 + 1] - g,
			db = screen.palette[i * 3 + 2] - b;
		int d = dr * dr + dg * dg + db * db;
		if (d < best_d) {
			best_d = d;
			best = i;
		}
	}
	return (uint8_t)best;
}

void fill_rect_colorref(int which, int x, int y, int w, int h, uint32_t colorref)
{
	uint8_t c = nearest_color(colorref);
	for (int row = y < 0 ? 0 : y; row < y + h && row < SCREEN_H; row++) {
		for (int col = x < 0 ? 0 : x; col < x + w && col < SCREEN_W; col++)
			screen.pixels[which][row * SCREEN_W + col] = c;
	}
}
