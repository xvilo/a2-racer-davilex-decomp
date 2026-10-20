#include "res/discimage.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define make_dir(p) _mkdir(p)
#else
#include <sys/stat.h>
#define make_dir(p) mkdir(p, 0755)
#endif

#define SECTOR 2048

typedef struct {
	FILE *f;
	int raw; /* 2352-byte sectors */
} disc_t;

static const uint8_t sync_pattern[12] = { 0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0 };

static int ends_with_ci(const char *s, const char *suffix)
{
	size_t n = strlen(s), m = strlen(suffix);
	if (n < m)
		return 0;
	for (size_t i = 0; i < m; i++) {
		if (tolower((unsigned char)s[n - m + i]) != suffix[i])
			return 0;
	}
	return 1;
}

int is_disc_image(const char *path)
{
	return ends_with_ci(path, ".bin") || ends_with_ci(path, ".iso") || ends_with_ci(path, ".img");
}

/* The 2048 user bytes of a sector. */
static int read_sector(disc_t *d, uint32_t lba, uint8_t *out)
{
	if (!d->raw) {
		if (fseek(d->f, (long)lba * SECTOR, SEEK_SET) != 0)
			return -1;
		return fread(out, 1, SECTOR, d->f) == SECTOR ? 0 : -1;
	}
	uint8_t s[2352];
	if (fseek(d->f, (long)lba * 2352, SEEK_SET) != 0 || fread(s, 1, sizeof s, d->f) != sizeof s)
		return -1;
	if (memcmp(s, sync_pattern, 12) != 0) {
		memset(out, 0, SECTOR);
		return 0;
	}
	/* Mode 1: data after the 16-byte header; Mode 2 XA form 1: after the subheader too. */
	memcpy(out, s + (s[15] == 1 ? 16 : 24), SECTOR);
	return 0;
}

static int read_extent(disc_t *d, uint32_t lba, uint32_t size, uint8_t *out)
{
	uint8_t sector[SECTOR];
	for (uint32_t done = 0; done < size; done += SECTOR, lba++) {
		if (read_sector(d, lba, sector))
			return -1;
		uint32_t n = size - done < SECTOR ? size - done : SECTOR;
		memcpy(out + done, sector, n);
	}
	return 0;
}

static uint32_t rd32(const uint8_t *p)
{
	return (uint32_t)(p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24);
}

int disc_image_unpack(const char *image, const char *out_dir)
{
	char path[1200];
	snprintf(path, sizeof path, "%s/A2MENU.DAT", out_dir);
	FILE *done = fopen(path, "rb");
	if (done) {
		fclose(done);
		return 0;
	}
	disc_t d = { fopen(image, "rb"), 0 };
	if (!d.f) {
		fprintf(stderr, "cannot open %s\n", image);
		return -1;
	}
	uint8_t head[12];
	d.raw = fread(head, 1, sizeof head, d.f) == sizeof head && !memcmp(head, sync_pattern, 12);
	/* The primary volume descriptor at sector 16: ISO 9660 names, upper case "NAME;1". */
	uint8_t pvd[SECTOR];
	if (read_sector(&d, 16, pvd) || pvd[0] != 1 || memcmp(pvd + 1, "CD001", 5) != 0) {
		fprintf(stderr, "%s: not an ISO 9660 CD image\n", image);
		fclose(d.f);
		return -1;
	}
	const uint8_t *root = pvd + 156;
	uint32_t dir_size = rd32(root + 10);
	uint8_t *dir = malloc(dir_size);
	if (!dir || read_extent(&d, rd32(root + 2), dir_size, dir)) {
		free(dir);
		fclose(d.f);
		return -1;
	}
	make_dir(out_dir);
	fprintf(stderr, "unpacking %s into %s (once)\n", image, out_dir);
	int files = 0, result = 0;
	for (uint32_t i = 0; i < dir_size && result == 0;) {
		uint8_t len = dir[i];
		if (len == 0) {
			i = (i / SECTOR + 1) * SECTOR; /* records don't cross sectors */
			continue;
		}
		const uint8_t *r = dir + i;
		i += len;
		uint8_t name_len = r[32];
		if (r[25] & 2 || name_len == 0 || (name_len == 1 && r[33] <= 1))
			continue; /* directories, "." and ".." */
		char name[256];
		memcpy(name, r + 33, name_len);
		name[name_len] = 0;
		char *semi = strchr(name, ';');
		if (semi)
			*semi = 0;
		size_t n = strlen(name);
		if (n && name[n - 1] == '.')
			name[n - 1] = 0;
		uint32_t size = rd32(r + 10);
		uint8_t *data = malloc(size ? size : 1);
		snprintf(path, sizeof path, "%s/%s", out_dir, name);
		FILE *o = data ? fopen(path, "wb") : NULL;
		if (!o || read_extent(&d, rd32(r + 2), size, data) || fwrite(data, 1, size, o) != size) {
			fprintf(stderr, "cannot unpack %s\n", name);
			result = -1;
		}
		if (o)
			fclose(o);
		free(data);
		files++;
	}
	free(dir);
	fclose(d.f);
	if (result == 0)
		fprintf(stderr, "unpacked %d files\n", files);
	else
		remove(path);
	return result;
}
