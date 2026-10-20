/* Minimal PNG writer for screenshots (indexed colour, uncompressed deflate). */
#ifndef A2_PNG_H
#define A2_PNG_H

#include <stdint.h>

/* Returns 0 on success. */
int png_write_indexed(const char *path, const uint8_t *pixels, int w, int h,
	const uint8_t palette[768]);

#endif
