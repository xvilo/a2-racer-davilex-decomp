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
#include "menu/help.h"
#include "menu/m_main.h"
#include "menu/m_start.h"
#include "menu/menu_data.h"
#include "menu/infio.h"
#include "menu/mysound.h"
#include "menu/optie.h"
#include "menu/score.h"
#include "menu/slot.h"
#include "menu/spel.h"
#include "movie/movies.h"
#include "race/race_main.h"
#include "race/race_view.h"
#include "platform/sys.h"
#include "res/discimage.h"
#include "xvga/fileio.h"
#include "xvga/font.h"
#include "xvga/inifile.h"
#include "xvga/sound.h"

static void save_front_screen(const char *path)
{
	png_write_indexed(path, screen.pixels[SCREEN_FRONT], SCREEN_W, SCREEN_H, screen.palette);
}

static void usage(void)
{
	fprintf(stderr,
		"usage: a2racer --data <disc directory> [options]\n"
		"  --data <dir>         the A2 Racer CD (or a copy of its files)\n"
		"  --save <dir>         where options and save slots go (default: user data folder)\n"
		"  --fonts <dir>        folder with ariblk.ttf (Arial Black) if not found otherwise\n"
		"  --scale <n>          window size as a multiple of 640x480\n"

		"  --race <stage>       drive a stage (1..6) straight away, without the menu\n"
		"  --fake-race <pos>    instead of driving, finish each race at this position\n"
		"                       (1..6) with made-up times, to test what follows\n"
		"  --race-view <stage>  fly along a stage's road (1..6; tests the 3D code)\n"
		"  --screenshot <file>  open the main menu, save it as PNG and exit\n"
		"  --script <file>      scripted input for testing (see platform/sys_sdl.c)\n");
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

/* win_open_menu: the settings, save slots, scores and INFIO.INI from disk. */
static void win_open_menu(void)
{
	clear_screen(0, 0);
	clear_screen(2, 0);
	clear_screen(1, 0);
	init_optie();
	init_bedien();
	init_slotdbase();
	init_scoredbase();
	init_infio();
	int last = get_last_saved_slot();
	if (slot_list[last].used)
		set_active_slot(last);
	set_help_key(0x0d);
	set_help_key(0x20);
	set_help_key(0x1b);
}

/* win_close_menu: everything back to disk, for the race and the next menu run. */
static void win_close_menu(void)
{
	clear_screen(0, 0);
	clear_screen(2, 0);
	clear_screen(1, 0);
	save_optie();
	save_bedien();
	update_active_slot_optie();
	save_slotdbase();
	save_scoredbase();
	save_infio();
	clear_help_keys();
}

/* win_handle_menu: one run of the menu. Coming back from a race, handle_game shows the
 * result first and goes on to the main menu, or to the player choice when the series
 * is over. Returns MENU_START_RACE or MENU_EXIT. */
static int win_handle_menu(game_t *g, int first)
{
	win_open_menu();
	if (first || !handle_game(g)) {
		if (!menu_start(g, first)) {
			win_close_menu();
			return MENU_EXIT;
		}
	}
	int r = menu_main(g);
	reset_infio();
	win_close_menu();
	return r;
}

/* A2SPEL.DAT's place: the race. With --fake-race, a finished race is written to
 * INFIO.INI instead, the player at `position` and the rivals in the other places. */
static void run_race(game_t *g, int position)
{
	if (position < 1 || position > 6) {
		race_main(g);
		return;
	}
	init_infio();
	int16_t *racer = &infio.racer_1;
	int32_t *time = &infio.time_racer_1;
	int place = 1;
	infio.race_finished = 1;
	infio.position = (int16_t)position;
	infio.time = 240 + position * 7;
	for (int r = 0; r < 5; r++, place++) {
		if (place == position)
			place++;
		racer[r] = (int16_t)place;
		time[r] = 240 + place * 7;
	}
	infio.money += 500;
	save_infio();
}

int main(int argc, char **argv)
{
	game_t g = { 0 };
	sys_options opt = { 1, false, NULL };
	const char *save_dir = NULL, *font_dir = NULL;
	/* The launcher passes FIRST on the first menu run of a session: with no players yet,
	 * that shows the logo intro. */
	int first = 1;
	int fake_race = 0;
	int view_stage = -1;
	int race_stage = -1;
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
		else if (!strcmp(argv[i], "--script") && i + 1 < argc)
			opt.script = argv[++i];
		else if (!strcmp(argv[i], "--race") && i + 1 < argc)
			race_stage = atoi(argv[++i]) - 1;
		else if (!strcmp(argv[i], "--race-view") && i + 1 < argc)
			view_stage = atoi(argv[++i]) - 1;
		else if (!strcmp(argv[i], "--fake-race") && i + 1 < argc)
			fake_race = atoi(argv[++i]);
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
	/* A CD image instead of a folder: its files are unpacked once into the user's data
	 * folder. */
	char disc_dir[1024];
	if (is_disc_image(g.data_dir)) {
		char *pref = SDL_GetPrefPath("Davilex", "A2 Racer");
		snprintf(disc_dir, sizeof disc_dir, "%sdisc", pref ? pref : "./");
		SDL_free(pref);
		if (disc_image_unpack(g.data_dir, disc_dir))
			return 1;
		g.data_dir = disc_dir;
	}
	if (named_archive_open(&g.menu_files, g.data_dir, "A2MRCS") ||
		sprite_archive_open(&g.menu_sprites, g.data_dir, "A2MRC")) {
		fprintf(stderr, "cannot read A2MRCS/A2MRC archives in %s\n", g.data_dir);
		return 1;
	}
	opt.hidden = g.screenshot != NULL;
	if (!sys_init("A2 Racer", &opt))
		return 1;
	sys_set_screenshot(save_front_screen);
	fileio_init(save_dir);
	forms_init(&g.menu_sprites, anims);
	font_init(font_dir);

	/* WinMain: sound (40 buffers) and the fonts, kept for the whole session. */
	if (g.screenshot || xvga_init_sound(0x28, &g.menu_files))
		mysnd_state = 1;
	load_menu_fonts();

	int result = 0;
	if (view_stage >= 0) {
		result = race_view(&g, view_stage);
	}
	else if (race_stage >= 0) {
		/* A2_RACE_REPEAT (testing): several races in one run, as the launcher's loop does. */
		const char *repeat = getenv("A2_RACE_REPEAT");
		for (int k = repeat ? atoi(repeat) : 1; k > 0 && !sys_quit_requested(); k--) {
			win_open_menu();
			reset_infio();
			infio.stage = (int16_t)race_stage;
			save_infio();
			result = race_main(&g);
			printf("race: finished %d, position %d, time %d s\n", infio.race_finished,
				infio.position, infio.time);
		}
	}
	else if (g.screenshot) {
		win_open_menu();
		mm_open_screen(&g);
		result = png_write_indexed(g.screenshot, screen.pixels[SCREEN_FRONT], SCREEN_W,
			SCREEN_H, screen.palette) ? 1 : 0;
	}
	else {
		/* The launcher (A2.EXE): the intro film, then the menu and the race in turn until
		 * the player leaves the menu, then the exit film. */
		play_movie(g.data_dir, "a2intro.smk");
		while (win_handle_menu(&g, first) == MENU_START_RACE && !sys_quit_requested()) {
			first = 0;
			run_race(&g, fake_race);
			/* After the last stage, won: the winner's film. */
			set_ini_filename("INFIO.INI");
			int stage = (int)read_ini_value_setting("GAME", "ETAPPE");
			int finished = (int)read_ini_value_setting("GAME", "RACE_KLAAR");
			int position = (int)read_ini_value_setting("GAME", "POSITIE");
			if (stage == 5 && finished && position)
				play_movie(g.data_dir, "a2win.smk");
		}
		if (!sys_quit_requested())
			play_movie(g.data_dir, "a2exit.smk");
	}
	font_shutdown();
	xvga_exit_sound();
	sys_shutdown();
	sprite_archive_close(&g.menu_sprites);
	named_archive_close(&g.menu_files);
	return result;
}
