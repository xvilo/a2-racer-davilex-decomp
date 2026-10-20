/* The main menu (m_main.cpp). */
#include "menu/m_main.h"

#include <stdio.h>
#include <string.h>

#include "gfx/screen.h"
#include "gfx/wipes.h"
#include "menu/forms.h"
#include "menu/menu_data.h"
#include "menu/mysound.h"
#include "menu/optie.h"
#include "menu/slot.h"
#include "menu/txttool.h"
#include "platform/sys.h"
#include "res/pcx.h"
#include "xvga/font.h"

/* Controls of mm_form, by index. */
enum {
	MM_HELP = 0,
	MM_EXIT = 1,     /* "Einde" */
	MM_START = 2,
	MM_CONTROLS = 3, /* "Bediening" */
	MM_OPTIONS = 4,  /* "Opties" */
	MM_HISCORE = 5,  /* "Score" */
	MM_RESTART = 6,  /* "Opnieuw": cancel the current race series */
	MM_NEW_PLAYER = 7,
};

static int mm_tune_buffer = -1;
static game_t *game;

/* load_pcx_file_1 flags: 1 the picture, 2 read the palette, 4 apply it. */
enum { PCX_IMAGE = 1, PCX_PALETTE = 2, PCX_SET_PALETTE = 4 };

static int load_pcx_file_1(int which, const char *name, int flags)
{
	size_t size;
	const uint8_t *data = named_archive_find(&game->menu_files, name, &size);
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

/* Waits for the input that started an action to finish (wait_*_pressed). */
static void wait_keys_released(void)
{
	sys_input in = { 0 };
	sys_poll(&in);
}

static int mm_af_image(int anim, int frame)
{
	if (frame < 0 || frame >= anims[anim].frame_count) {
		fprintf(stderr, "mm_af_image: anim %d has no frame %d\n", anim, frame);
		return anims[anim].frames[0].image;
	}
	return anims[anim].frames[frame].image;
}

/* The player's name, money, total time, controls and display in the box on the right,
 * drawn at an offset (three passes give the shadow). */
static void mm_draw_info_text(int dx, int dy)
{
	int slot = slot_check_index(active_slot);
	int x = dx + 0x205;
	win_bprintf(1, x, dy + 0x8c, 0x5a, "%-16s", slot_naam_string(slot));
	win_print_xy(1, x, dy + 0x9d, "%-9d", slot_list[slot].money);
	win_print_xy(1, x, dy + 0xae, "%-6s", slot_tijd_string(slot));
	const char *controls;
	switch (control.control_type) {
	case 0: controls = "Toetsenbord"; break; /* keyboard */
	case 1: controls = "Joypad 1"; break;
	case 2: controls = "Stuur"; break;       /* steering wheel */
	default: controls = "Onbekend"; break;   /* unknown */
	}
	win_bprintf(1, dx + 0x205, dy + 0xbf, 0x5a, "%s", controls);
	const char *display;
	switch (opties.display_mode) {
	case 0: display = "3D Accelerator"; break;
	case 1: display = "320 x 200"; break;
	case 2: display = "640 x 480"; break;
	default: display = "Onbekend"; break;
	}
	win_bprintf(1, dx + 0x205, dy + 0xd1, 0x5a, "%s", display);
}

static void mm_display_info(void)
{
	xvga_select_font(4);
	xvga_set_font_colors(4, 0, 0x28f0ff);
	blt_scr_2_scr(2, 1, 500, 0x87, 0x262, 0xeb, 0);
	xvga_set_font_colors(4, 0, 0);
	mm_draw_info_text(0, 0);
	mm_draw_info_text(-1, -1);
	xvga_set_font_colors(4, 0, 0xf5f5f5);
	mm_draw_info_text(-2, -2);
	blt_scr_2_scr(1, 0, 500, 0x87, 0x262, 0xeb, 0);
	xvga_show();
}

/* The six buttons on the left slide in, one step every 33 ms. */
static void mm_animate_buttons(void)
{
	static const int anim[6] = { 0x1a, 0x1b, 0x1c, 0x1e, 0x1d, 0x1f };
	static const int y[6] = { 0x7c, 0xb0, 0xe4, 0x116, 0x14b, 0x17e };
	for (int step = 0; step < 12; step++) {
		uint32_t t = sys_ticks();
		for (int b = 0; b < 6; b++)
			set_mem_image(0, mm_af_image(anim[b], mm_entree_anim_tabel[b][step]), 0, y[b],
				MI_TOPLEFT);
		xvga_show();
		sys_input in = { 0 };
		do {
			sys_poll(&in); /* handle_win_msg */
			sys_sleep(1);
		} while (sys_ticks() - t < 0x21);
	}
}

/* The map of the Netherlands with the current stage. */
static void mm_animate_map(int stage)
{
	int image = mm_af_image(0x26, stage);
	const sprite *s = get_mem_image(image);
	if (!s)
		return;
	set_mem_image(1, image, 0x15d, 0xf5, MI_TOPLEFT);
	blt_scr_2_scr(1, 0, 0x15d, 0xf5, s->width + 0x15d, s->height + 0xf5, 0);
	xvga_show();
}

/* first: coming from the start (with the palette) rather than back from a submenu. */
static void mm_open_screen_1(int first)
{
	load_pcx_file_1(2, "hoofd.pcx", first ? 0x1f : 0x19);
	handle_wipe(2, WIPE_PIXELFADE, 600);
	blt_scr_2_scr(2, 0, 0, 0, SCREEN_W, SCREEN_H, 0); /* do_screen_update */
	mm_display_info();
	mm_animate_buttons();
	mm_animate_map(slot_list[slot_check_index(active_slot)].stage);
	activate_form(mm_form);
	xvga_show();
}

void mm_open_screen(game_t *g)
{
	game = g;
	mm_open_screen_1(1);
}

static void mm_close_screen(int clear)
{
	wait_keys_released();
	destroy_form();
	if (clear) {
		clear_screen(2, 0);
		handle_wipe(2, WIPE_PIXELFADE, 600);
	}
}

int menu_main(game_t *g)
{
	int result = MENU_EXIT;
	int click = -1;
	int running = 1;
	game = g;
	if (opties.sound == SOUND_ALL || opties.sound == SOUND_MUSIC_ONLY)
		snd_play_looping(&mm_tune_buffer, mm_tune[slot_list[slot_check_index(active_slot)].stage]);
	mm_open_screen_1(1);
	sys_input in = { 0 };
	while (running) {
		sys_poll(&in);
		int r = handle_form(&in);
		switch (r) {
		case FORM_QUIT:
			running = 0;
			break;
		case MM_HELP:
			snd_play(&click, "click1.wav");
			snd_wait(&click);
			/* TODO: play_help(mm_help) with the music turned down. */
			fprintf(stderr, "menu_main: help is not ported yet\n");
			break;
		case MM_EXIT:
			snd_play(&click, "click1.wav");
			running = 0;
			break;
		case MM_START:
			snd_play(&click, "click1.wav");
			snd_wait(&click);
			result = MENU_START_RACE;
			running = 0;
			break;
		case MM_CONTROLS:
		case MM_OPTIONS:
		case MM_HISCORE:
		case MM_RESTART:
		case MM_NEW_PLAYER:
			snd_play(&click, "click1.wav");
			fprintf(stderr, "menu_main: control %d is not ported yet\n", r);
			break;
		case FORM_ESCAPE:
			/* Escape moves to "Einde" and leaves. */
			focus_control(MM_EXIT);
			snd_play(&click, "click1.wav");
			running = 0;
			break;
		case FORM_UP:
		case FORM_DOWN: {
			/* Arrow keys cycle through the buttons on the left, Start to New player. */
			snd_play(&click, "click3.wav");
			int f = get_control_focus();
			if (r == FORM_DOWN)
				f = f >= MM_START && f < MM_NEW_PLAYER ? f + 1 : MM_START;
			else
				f = f > MM_START && f <= MM_NEW_PLAYER ? f - 1 : MM_NEW_PLAYER;
			focus_control(f);
			break;
		}
		default:
			break;
		}
		xvga_show();
		sys_sleep(10);
	}
	snd_wait(&click);
	mm_close_screen(1);
	snd_stop(&mm_tune_buffer);
	return result;
}
