/* A2 Racer port: entry point. Takes the place of A2.EXE (the launcher), which ran the
 * menu (A2MENU.DAT) and race (A2SPEL.DAT) as separate programs. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#include "game.h"
#include "gfx/png.h"
#include "gfx/screen.h"
#include "menu/forms.h"
#include "menu/m_main.h"
#include "menu/menu_data.h"
#include "platform/sys.h"

static void usage(void)
{
	fprintf(stderr,
		"usage: a2racer --data <disc directory> [--scale N] [--screenshot file.png]\n"
		"  --data        the A2 Racer CD (or a copy of its files)\n"
		"  --scale       window size as a multiple of 640x480\n"
		"  --screenshot  draw the main menu, save it as PNG and exit\n");
}

int main(int argc, char **argv)
{
	game_t g = { 0 };
	sys_options opt = { 1, false };
	g.data_dir = getenv("A2_DATA");
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--data") && i + 1 < argc)
			g.data_dir = argv[++i];
		else if (!strcmp(argv[i], "--scale") && i + 1 < argc)
			opt.scale = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--screenshot") && i + 1 < argc)
			g.screenshot = argv[++i];
		else {
			usage();
			return 2;
		}
	}
	if (!g.data_dir) {
		usage();
		return 2;
	}
	if (named_archive_open(&g.menu_files, g.data_dir, "A2MRCS") ||
		sprite_archive_open(&g.menu_sprites, g.data_dir, "A2MRC")) {
		fprintf(stderr, "cannot read A2MRCS/A2MRC archives in %s\n", g.data_dir);
		return 1;
	}
	opt.hidden = g.screenshot != NULL;
	if (!sys_init("A2 Racer", &opt))
		return 1;
	forms_init(&g.menu_sprites, anims);

	int result = 0;
	if (g.screenshot) {
		mm_open_screen(&g);
		screen_copy(SCREEN_FRONT, SCREEN_BACK);
		draw_form(SCREEN_FRONT);
		result = png_write_indexed(g.screenshot, screen.pixels[SCREEN_FRONT], SCREEN_W,
			SCREEN_H, screen.palette) ? 1 : 0;
	}
	else {
		int r = menu_main(&g);
		if (r == MENU_START_RACE)
			printf("start race: the race (A2SPEL) is not ported yet\n");
	}
	sys_shutdown();
	sprite_archive_close(&g.menu_sprites);
	named_archive_close(&g.menu_files);
	return result;
}
