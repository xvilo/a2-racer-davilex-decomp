/* The 640x480 8-bit screen, the port's stand-in for the xvga library's DirectDraw
 * surfaces (initlib.cpp): a front screen shown to the player and back/buffer screens
 * drawn into off-screen, all sharing one 256-colour palette. */
#ifndef A2_SCREEN_H
#define A2_SCREEN_H

#include <stdint.h>

#include "res/archive.h"

#define SCREEN_W 640
#define SCREEN_H 480

enum { SCREEN_FRONT, SCREEN_BACK, SCREEN_BUFFER, SCREEN_COUNT };

typedef struct {
	uint8_t pixels[SCREEN_COUNT][SCREEN_W * SCREEN_H];
	uint8_t palette[768];
} screen_t;

extern screen_t screen;

void screen_clear(int which, uint8_t color);
void screen_copy(int dst, int src);
/* Draws an image with its top-left corner at x, y, clipped to the screen. With
 * transparent >= 0 that palette index is skipped. */
void screen_blit(int which, const uint8_t *pixels, int w, int h, int x, int y, int transparent);
void screen_blit_sprite(int which, const sprite *s, int x, int y);

/* xvga library blits (clipblit.cpp), under their original names. */

/* Palette index that set_mem_image leaves out (blt_sprite skips 0xFF). */
#define MEM_IMAGE_TRANSPARENT 0xFF
/* set_mem_image flags: 1 clip to the clip limits, 2 x/y is the top-left corner instead
 * of the image's center point. */
enum { MI_CLIP = 1, MI_TOPLEFT = 2 };

/* The menu sprites (A2MRC) used as mem images, numbered from 1. */
void set_mem_images(const sprite_archive *images);
const sprite *get_mem_image(int index);
void set_mem_image(int which, int image, int x, int y, int flags);
/* Copies the rectangle (x1,y1)-(x2,y2) of screen src to the same place in dst. */
void blt_scr_2_scr(int src, int dst, int x1, int y1, int x2, int y2, int flags);
void clear_screen(int which, uint8_t color);
void set_clip_limit(int x1, int y1, int x2, int y2);
/* Shows the front screen (the original drew straight into the visible surface). */
void xvga_show(void);

#endif
