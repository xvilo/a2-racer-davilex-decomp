/* Minimal PNG writer for screenshots (indexed or RGB colour, uncompressed deflate). */
#ifndef A2_PNG_H
#define A2_PNG_H

#include <stdint.h>

/* Returns 0 on success. */
int png_write_indexed(const char *path, const uint8_t *pixels, int w, int h,
	const uint8_t palette[768]);
int png_write_rgb(const char *path, const uint8_t *rgb, int w, int h);

#endif
