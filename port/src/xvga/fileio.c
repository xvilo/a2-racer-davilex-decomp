#include "xvga/fileio.h"

#include <stdio.h>
#include <string.h>

#include <SDL.h>

static char save_dir[1024];

void fileio_init(const char *dir)
{
	if (dir) {
		snprintf(save_dir, sizeof save_dir, "%s", dir);
		size_t n = strlen(save_dir);
		if (n && save_dir[n - 1] != '/' && save_dir[n - 1] != '\\')
			snprintf(save_dir + n, sizeof save_dir - n, "/");
		return;
	}
	char *pref = SDL_GetPrefPath("Davilex", "A2 Racer");
	snprintf(save_dir, sizeof save_dir, "%s", pref ? pref : "./");
	SDL_free(pref);
}

const char *fileio_save_dir(void)
{
	return save_dir;
}

const char *save_path(const char *name)
{
	static char path[1200];
	snprintf(path, sizeof path, "%s%s", save_dir, name);
	return path;
}

int file_exist(const char *name)
{
	FILE *f = fopen(save_path(name), "rb");
	if (!f)
		return 0;
	fclose(f);
	return 1;
}

int file_read(void *buf, size_t size, const char *name)
{
	FILE *f = fopen(save_path(name), "rb");
	if (!f)
		return 1;
	size_t n = fread(buf, 1, size, f);
	fclose(f);
	return n == size ? 0 : 1;
}

int file_write(const void *buf, size_t size, const char *name)
{
	FILE *f = fopen(save_path(name), "wb");
	if (!f)
		return 1;
	size_t n = fwrite(buf, 1, size, f);
	return fclose(f) == 0 && n == size ? 0 : 1;
}

void file_delete(const char *name)
{
	remove(save_path(name));
}
