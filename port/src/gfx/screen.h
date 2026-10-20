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

#endif
