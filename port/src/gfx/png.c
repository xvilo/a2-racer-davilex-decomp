#include "gfx/png.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t crc_table[256];

static uint32_t crc32_update(uint32_t crc, const uint8_t *p, size_t n)
{
	if (!crc_table[1]) {
		for (uint32_t i = 0; i < 256; i++) {
			uint32_t c = i;
			for (int k = 0; k < 8; k++)
				c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
			crc_table[i] = c;
		}
	}
	crc = ~crc;
	while (n--)
		crc = crc_table[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
	return ~crc;
}

static void put32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)(v >> 24);
	p[1] = (uint8_t)(v >> 16);
	p[2] = (uint8_t)(v >> 8);
	p[3] = (uint8_t)v;
}

static void chunk(FILE *f, const char *tag, const uint8_t *body, uint32_t n)
{
	uint8_t head[8];
	put32(head, n);
	memcpy(head + 4, tag, 4);
	fwrite(head, 1, 8, f);
	if (n)
		fwrite(body, 1, n, f);
	uint32_t crc = crc32_update(crc32_update(0, (const uint8_t *)tag, 4), body, n);
	put32(head, crc);
	fwrite(head, 1, 4, f);
}

/* `bpp` bytes per pixel: 1 indexed (with palette) or 3 RGB. */
static int png_write(const char *path, const uint8_t *pixels, int w, int h, int bpp,
	const uint8_t palette[768])
{
	FILE *f = fopen(path, "wb");
	if (!f)
		return -1;
	static const uint8_t sig[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };
	fwrite(sig, 1, 8, f);
	uint8_t ihdr[13];
	put32(ihdr, (uint32_t)w);
	put32(ihdr + 4, (uint32_t)h);
	ihdr[8] = 8;  /* bit depth */
	ihdr[9] = bpp == 1 ? 3 : 2;  /* indexed colour or RGB */
	ihdr[10] = ihdr[11] = ihdr[12] = 0;
	chunk(f, "IHDR", ihdr, 13);
	if (bpp == 1)
		chunk(f, "PLTE", palette, 768);

	/* zlib stream of stored (uncompressed) deflate blocks. */
	size_t stride = (size_t)w * bpp;
	size_t raw_len = (stride + 1) * h;
	size_t blocks = (raw_len + 65534) / 65535;
	size_t z_len = 2 + raw_len + blocks * 5 + 4;
	uint8_t *z = malloc(z_len), *p = z;
	if (!z) {
		fclose(f);
		return -1;
	}
	*p++ = 0x78;
	*p++ = 0x01;
	uint32_t a = 1, b = 0;
	size_t done = 0;
	size_t row = 0;
	long col = -1; /* col -1: the row's filter byte */
	while (done < raw_len) {
		size_t n = raw_len - done > 65535 ? 65535 : raw_len - done;
		*p++ = done + n == raw_len;
		*p++ = (uint8_t)n;
		*p++ = (uint8_t)(n >> 8);
		*p++ = (uint8_t)~n;
		*p++ = (uint8_t)(~n >> 8);
		for (size_t i = 0; i < n; i++) {
			uint8_t v = col < 0 ? 0 : pixels[row * stride + (size_t)col];
			if (++col == (long)stride) {
				col = -1;
				row++;
			}
			*p++ = v;
			a = (a + v) % 65521;
			b = (b + a) % 65521;
		}
		done += n;
	}
	put32(p, b << 16 | a);
	p += 4;
	chunk(f, "IDAT", z, (uint32_t)(p - z));
	free(z);
	chunk(f, "IEND", NULL, 0);
	return fclose(f) == 0 ? 0 : -1;
}

int png_write_indexed(const char *path, const uint8_t *pixels, int w, int h,
	const uint8_t palette[768])
{
	return png_write(path, pixels, w, h, 1, palette);
}

int png_write_rgb(const char *path, const uint8_t *rgb, int w, int h)
{
	return png_write(path, rgb, w, h, 3, NULL);
}
