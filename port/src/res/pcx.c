#include "res/pcx.h"

#include <stdlib.h>
#include <string.h>

int pcx_decode(const uint8_t *data, size_t size, pcx_image *out)
{
	memset(out, 0, sizeof *out);
	if (size < 128 + 769 || data[0] != 10 || data[3] != 8 || data[65] != 1)
		return -1;
	int x0 = data[4] | data[5] << 8, y0 = data[6] | data[7] << 8;
	int x1 = data[8] | data[9] << 8, y1 = data[10] | data[11] << 8;
	int bpl = data[66] | data[67] << 8;
	int w = x1 - x0 + 1, h = y1 - y0 + 1;
	if (w <= 0 || h <= 0 || bpl < w)
		return -1;
	uint8_t *line = malloc((size_t)bpl);
	out->pixels = malloc((size_t)w * h);
	if (!line || !out->pixels) {
		free(line);
		free(out->pixels);
		return -1;
	}
	size_t i = 128, end = size - 769;
	for (int y = 0; y < h; y++) {
		int n = 0;
		while (n < bpl && i < end) {
			uint8_t b = data[i++];
			if (data[2] == 1 && b >= 0xC0) {
				int run = b & 0x3F;
				uint8_t v = i < end ? data[i++] : 0;
				while (run-- && n < bpl)
					line[n++] = v;
			}
			else {
				line[n++] = b;
			}
		}
		memset(line + n, 0, (size_t)(bpl - n));
		memcpy(out->pixels + (size_t)y * w, line, (size_t)w);
	}
	free(line);
	out->width = w;
	out->height = h;
	if (data[size - 769] == 12)
		memcpy(out->palette, data + size - 768, 768);
	return 0;
}

void pcx_free(pcx_image *img)
{
	free(img->pixels);
	img->pixels = NULL;
}
