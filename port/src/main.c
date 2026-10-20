/* A2 Racer port: entry point. Takes the place of A2.EXE (the launcher), which ran the
 * menu (A2MENU.DAT) and race (A2SPEL.DAT) as separate programs; the start-up follows
 * A2MENU.DAT's WinMain and win_open_menu. */
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
#include "menu/mysound.h"
#include "menu/optie.h"
#include "menu/slot.h"
#include "platform/sys.h"
#include "xvga/fileio.h"
#include "xvga/font.h"
#include "xvga/sound.h"

static void usage(void)
{
	fprintf(stderr,
		"usage: a2racer --data <disc directory> [options]\n"
		"  --data <dir>         the A2 Racer CD (or a copy of its files)\n"
		"  --save <dir>         where options and save slots go (default: user data folder)\n"
		"  --fonts <dir>        folder with ariblk.ttf (Arial Black) if not found otherwise\n"
		"  --scale <n>          window size as a multiple of 640x480\n"
		"  --screenshot <file>  open the main menu, save it as PNG and exit\n");
}

/* win_open_menu's fonts: Arial Black in five sizes, transparent background. */
static void load_menu_fonts(void)
{
	static const int height[5] = { 0xd, 0x14, 0x17, 0x12, 0x12 };
	for (int i = 0; i < 5; i++) {
		xvga_load_font(i, "Arial Black", height[i], 0, 0);
		xvga_set_font_colors(i, 0, 0xffffff);
		xvga_set_font_bkmode(i, 1);
	}
}

int main(int argc, char **argv)
{
	game_t g = { 0 };
	sys_options opt = { 1, false };
	const char *save_dir = NULL, *font_dir = NULL;
	g.data_dir = getenv("A2_DATA");
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--data") && i + 1 < argc)
			g.data_dir = argv[++i];
		else if (!strcmp(argv[i], "--save") && i + 1 < argc)
			save_dir = argv[++i];
		else if (!strcmp(argv[i], "--fonts") && i + 1 < argc)
			font_dir = argv[++i];
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
	fileio_init(save_dir);
	forms_init(&g.menu_sprites, anims);
	font_init(font_dir);

	/* WinMain: sound (40 buffers), then win_open_menu: settings, slots, fonts. */
	if (g.screenshot || xvga_init_sound(0x28, &g.menu_files))
		mysnd_state = 1;
	init_optie();
	init_bedien();
	init_slotdbase();
	int last = get_last_saved_slot();
	if (slot_list[last].used)
		set_active_slot(last);
	load_menu_fonts();

	int result = 0;
	if (g.screenshot) {
		mm_open_screen(&g);
		result = png_write_indexed(g.screenshot, screen.pixels[SCREEN_FRONT], SCREEN_W,
			SCREEN_H, screen.palette) ? 1 : 0;
	}
	else {
		int r = menu_main(&g);
		if (r == MENU_START_RACE)
			printf("start race: the race (A2SPEL) is not ported yet\n");
		/* win_close_menu */
		save_optie();
		save_bedien();
		update_active_slot_optie();
		save_slotdbase();
	}
	font_shutdown();
	xvga_exit_sound();
	sys_shutdown();
	sprite_archive_close(&g.menu_sprites);
	named_archive_close(&g.menu_files);
	return result;
}
