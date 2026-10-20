#include "res/archive.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

static int name_equal_ci(const char *a, const char *b)
{
	for (; *a && *b; a++, b++) {
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
			return 0;
	}
	return *a == *b;
}

static uint8_t *read_path(const char *path, size_t *size)
{
	FILE *f = fopen(path, "rb");
	if (!f)
		return NULL;
	fseek(f, 0, SEEK_END);
	long n = ftell(f);
	fseek(f, 0, SEEK_SET);
	uint8_t *buf = malloc(n > 0 ? (size_t)n : 1);
	if (buf && fread(buf, 1, (size_t)n, f) != (size_t)n) {
		free(buf);
		buf = NULL;
	}
	fclose(f);
	if (buf && size)
		*size = (size_t)n;
	return buf;
}

/* The disc is ISO9660 (upper case); copies on other file systems may differ in case. */
uint8_t *read_file_ci(const char *dir, const char *name, size_t *size)
{
	char path[1024];
	snprintf(path, sizeof path, "%s/%s", dir, name);
	uint8_t *buf = read_path(path, size);
	if (buf)
		return buf;
#ifdef _WIN32
	return NULL; /* the Windows file system already ignores case */
#else
	DIR *d = opendir(dir);
	if (!d)
		return NULL;
	struct dirent *e;
	while ((e = readdir(d)) != NULL) {
		if (name_equal_ci(e->d_name, name)) {
			snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
			buf = read_path(path, size);
			break;
		}
	}
	closedir(d);
	return buf;
#endif
}

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t rd32(const uint8_t *p) { return (uint32_t)(p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24); }

static int load_pair(const char *dir, const char *name, uint8_t **ind, size_t *ind_size,
	uint8_t **img, size_t *img_size)
{
	char file[64];
	snprintf(file, sizeof file, "%s.IND", name);
	*ind = read_file_ci(dir, file, ind_size);
	snprintf(file, sizeof file, "%s.IMG", name);
	*img = read_file_ci(dir, file, img_size);
	if (!*ind || !*img || *ind_size < 2) {
		free(*ind);
		free(*img);
		return -1;
	}
	return 0;
}

static int cmp_offset(const void *a, const void *b)
{
	uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
	return x < y ? -1 : x > y;
}

int named_archive_open(named_archive *a, const char *dir, const char *name)
{
	uint8_t *ind;
	size_t ind_size;
	memset(a, 0, sizeof *a);
	if (load_pair(dir, name, &ind, &ind_size, &a->data, &a->data_size))
		return -1;
	int n = rd16(ind);
	if (ind_size < 2 + (size_t)n * 24) {
		free(ind);
		free(a->data);
		return -1;
	}
	a->entries = calloc((size_t)n, sizeof *a->entries);
	uint32_t *ends = malloc(((size_t)n + 1) * sizeof *ends);
	for (int i = 0; i < n; i++) {
		const uint8_t *e = ind + 2 + 24 * i;
		memcpy(a->entries[i].name, e, 15);
		a->entries[i].name[15] = 0;
		a->entries[i].offset = rd32(e + 20);
		ends[i] = a->entries[i].offset;
	}
	ends[n] = (uint32_t)a->data_size;
	qsort(ends, (size_t)n + 1, sizeof *ends, cmp_offset);
	/* An entry runs up to the next entry's offset. */
	for (int i = 0; i < n; i++) {
		uint32_t end = (uint32_t)a->data_size;
		for (int k = 0; k <= n; k++) {
			if (ends[k] > a->entries[i].offset) {
				end = ends[k];
				break;
			}
		}
		a->entries[i].size = end - a->entries[i].offset;
	}
	free(ends);
	free(ind);
	a->count = n;
	return 0;
}

void named_archive_close(named_archive *a)
{
	free(a->data);
	free(a->entries);
	memset(a, 0, sizeof *a);
}

const uint8_t *named_archive_find(const named_archive *a, const char *file, size_t *size)
{
	for (int i = 0; i < a->count; i++) {
		if (name_equal_ci(a->entries[i].name, file)) {
			if (size)
				*size = a->entries[i].size;
			return a->data + a->entries[i].offset;
		}
	}
	return NULL;
}

int sprite_archive_open(sprite_archive *a, const char *dir, const char *name)
{
	uint8_t *ind;
	size_t ind_size;
	memset(a, 0, sizeof *a);
	if (load_pair(dir, name, &ind, &ind_size, &a->data, &a->data_size))
		return -1;
	int n = rd16(ind);
	if (ind_size < 2 + (size_t)n * 4) {
		free(ind);
		free(a->data);
		return -1;
	}
	a->sprites = calloc((size_t)n, sizeof *a->sprites);
	for (int i = 0; i < n; i++) {
		uint32_t off = rd32(ind + 2 + 4 * i);
		if ((size_t)off + 8 > a->data_size)
			continue;
		const uint8_t *p = a->data + off;
		sprite *s = &a->sprites[i];
		s->width = rd16(p);
		s->height = rd16(p + 2);
		s->center_x = rd16(p + 4);
		s->center_y = rd16(p + 6);
		if ((size_t)off + 8 + (size_t)s->width * s->height > a->data_size) {
			s->width = s->height = 0;
			continue;
		}
		s->pixels = p + 8;
	}
	free(ind);
	a->count = n;
	return 0;
}

void sprite_archive_close(sprite_archive *a)
{
	free(a->data);
	free(a->sprites);
	memset(a, 0, sizeof *a);
}
