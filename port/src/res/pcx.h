/* 8-bit run-length PCX pictures with a 256-colour palette (rcpcx.cpp in the original). */
#ifndef A2_PCX_H
#define A2_PCX_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
	int width, height;
	uint8_t *pixels;      /* width * height palette indices */
	uint8_t palette[768]; /* RGB, 8 bits per channel */
} pcx_image;

/* Returns 0 on success; free with pcx_free. */
int pcx_decode(const uint8_t *data, size_t size, pcx_image *out);
void pcx_free(pcx_image *img);

#endif
