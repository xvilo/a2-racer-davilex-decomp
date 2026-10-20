#include "race/texture.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#include "platform/gl.h"
#include "race/race_files.h"
#include "res/pcx.h"

texture_t *texture_table;
static int texture_capacity, texture_count;
static int filter_linear;

int alloc_textures(int count)
{
	texture_table = calloc((size_t)count, sizeof *texture_table);
	if (!texture_table)
		return 2000;
	texture_capacity = count;
	texture_count = 0;
	return 0;
}

void free_textures(void)
{
	for (int i = 0; i < texture_count; i++)
		glDeleteTextures(1, &texture_table[i].gl);
	free(texture_table);
	texture_table = NULL;
	texture_capacity = texture_count = 0;
}

static void apply_filter(void)
{
	GLint f = filter_linear ? GL_LINEAR : GL_NEAREST;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, f);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, f);
}

void set_texture_filter(int linear)
{
	filter_linear = linear;
	for (int i = 0; i < texture_count; i++) {
		glBindTexture(GL_TEXTURE_2D, texture_table[i].gl);
		apply_filter();
	}
}

int load_texture(const char *filename, int flags)
{
	(void)flags;
	for (int i = 0; i < texture_count; i++) {
		if (!SDL_strcasecmp(texture_table[i].name, filename))
			return i;
	}
	if (texture_count == texture_capacity) {
		fprintf(stderr, "no more free textures (%s)\n", filename);
		exit(1);
	}
	size_t size;
	const uint8_t *data = race_file(filename, &size);
	pcx_image img;
	if (!data || pcx_decode(data, size, &img)) {
		fprintf(stderr, "cannot load texture %s\n", filename);
		exit(1);
	}
	texture_t *t = &texture_table[texture_count];
	SDL_strlcpy(t->name, filename, sizeof t->name);
	t->width = img.width;
	t->height = img.height;
	uint8_t *rgba = malloc((size_t)img.width * img.height * 4);
	for (int i = 0; i < img.width * img.height; i++) {
		uint8_t c = img.pixels[i];
		memcpy(&rgba[i * 4], &img.palette[c * 3], 3);
		rgba[i * 4 + 3] = c == 255 ? 0 : 255;
		if (c == 255)
			t->transparent = 1;
	}
	glGenTextures(1, &t->gl);
	glBindTexture(GL_TEXTURE_2D, t->gl);
	apply_filter();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img.width, img.height, 0, GL_RGBA,
		GL_UNSIGNED_BYTE, rgba);
	free(rgba);
	pcx_free(&img);
	return texture_count++;
}
