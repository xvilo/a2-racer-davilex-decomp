#include "menu/m_main.h"

#include <stdio.h>
#include <string.h>

#include "gfx/screen.h"
#include "menu/forms.h"
#include "menu/menu_data.h"
#include "platform/sys.h"
#include "res/pcx.h"

/* Controls of mm_form, by index. */
enum {
	MM_HELP = 0,
	MM_EXIT = 1,     /* "Einde" */
	MM_START = 2,
	MM_CONTROLS = 3, /* "Bediening" */
	MM_OPTIONS = 4,  /* "Opties" */
	MM_HISCORE = 5,
	MM_RESTART = 6,  /* "Opnieuw": cancel the current race series */
	MM_NEW_PLAYER = 7,
};

/* Loads a menu picture into a screen and takes over its palette (load_pcx_file). */
static int load_screen_pcx(game_t *g, int which, const char *name)
{
	size_t size;
	const uint8_t *data = named_archive_find(&g->menu_files, name, &size);
	pcx_image img;
	if (!data || pcx_decode(data, size, &img)) {
		fprintf(stderr, "cannot load %s\n", name);
		return -1;
	}
	screen_blit(which, img.pixels, img.width, img.height, 0, 0, -1);
	memcpy(screen.palette, img.palette, sizeof screen.palette);
	pcx_free(&img);
	return 0;
}

/* TODO from the original: the wipe transition, the slot info text (mm_display_info),
 * the entry animation of the buttons (mm_animate_buttons), the stage map
 * (mm_animate_map) and the menu music. */
void mm_open_screen(game_t *g)
{
	load_screen_pcx(g, SCREEN_BACK, "hoofd.pcx");
	activate_form(mm_form);
}

int menu_main(game_t *g)
{
	sys_input in = { 0 };
	mm_open_screen(g);
	for (;;) {
		sys_poll(&in);
		int r = handle_form(&in);
		switch (r) {
		case FORM_QUIT:
		case MM_EXIT:
			return MENU_EXIT;
		case FORM_ESCAPE:
			/* Escape moves to "Einde"; it does not leave by itself. */
			focus_control(MM_EXIT);
			break;
		case MM_START:
			return MENU_START_RACE;
		case FORM_UP:
		case FORM_DOWN: {
			/* Arrow keys cycle through the buttons on the left, Start to New player. */
			int f = get_control_focus();
			if (f < MM_START || f > MM_NEW_PLAYER)
				f = r == FORM_UP ? MM_NEW_PLAYER : MM_START;
			else if (r == FORM_UP)
				f = f == MM_START ? MM_NEW_PLAYER : f - 1;
			else
				f = f == MM_NEW_PLAYER ? MM_START : f + 1;
			focus_control(f);
			break;
		}
		case MM_HELP:
		case MM_CONTROLS:
		case MM_OPTIONS:
		case MM_HISCORE:
		case MM_RESTART:
		case MM_NEW_PLAYER:
			fprintf(stderr, "menu_main: control %d is not ported yet\n", r);
			break;
		default:
			break;
		}
		screen_copy(SCREEN_FRONT, SCREEN_BACK);
		draw_form(SCREEN_FRONT);
		sys_present(screen.pixels[SCREEN_FRONT], screen.palette);
		sys_sleep(10);
	}
}
