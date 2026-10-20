#include "menu/menu_util.h"

#include <stdio.h>
#include <string.h>

#include "gfx/screen.h"
#include "res/pcx.h"

int load_pcx_file_1(game_t *g, int which, const char *name, int flags)
{
	size_t size;
	const uint8_t *data = named_archive_find(&g->menu_files, name, &size);
	pcx_image img;
	if (!data || pcx_decode(data, size, &img)) {
		fprintf(stderr, "load_pcx_file: cannot load %s\n", name);
		return 1000;
	}
	if (flags & PCX_IMAGE)
		screen_blit(which, img.pixels, img.width, img.height, 0, 0, -1);
	if ((flags & (PCX_PALETTE | PCX_SET_PALETTE)) == (PCX_PALETTE | PCX_SET_PALETTE))
		memcpy(screen.palette, img.palette, sizeof screen.palette);
	pcx_free(&img);
	return 0;
}
