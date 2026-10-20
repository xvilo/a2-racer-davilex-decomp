/* Textures (texture.cpp, material.cpp). The original loaded each PCX into a DirectDraw
 * surface with a colour key and got a Direct3D texture handle; the port uploads an
 * OpenGL texture, palette index 255 (magenta in the game's pictures) transparent. */
#ifndef A2_TEXTURE_H
#define A2_TEXTURE_H

typedef struct {
	char name[32];
	unsigned gl;      /* OpenGL texture name */
	int width, height;
	int transparent;  /* has see-through pixels */
} texture_t;

extern texture_t *texture_table;

int alloc_textures(int count);
void free_textures(void);
/* Loads (or finds the already loaded) PCX texture; returns its index. */
int load_texture(const char *filename, int flags);
/* Bilinear filtering instead of the original's nearest pixel. */
void set_texture_filter(int linear);

#endif
