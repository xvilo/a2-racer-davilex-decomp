/* The main menu (m_main.cpp). */
#include "menu/m_main.h"

#include <stdio.h>
#include <string.h>

#include "gfx/screen.h"
#include "gfx/wipes.h"
#include "menu/forms.h"
#include "menu/help.h"
#include "menu/menu_data.h"
#include "menu/menu_util.h"
#include "menu/m_bedien.h"
#include "menu/m_nsplr.h"
#include "menu/m_opties.h"
#include "menu/m_score.h"
#include "menu/mysound.h"
#include "menu/optie.h"
#include "menu/requesters.h"
#include "menu/score.h"
#include "menu/slot.h"
#include "menu/txttool.h"
#include "platform/sys.h"
#include "xvga/fileio.h"
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

static void mm_close_screen(int clear);

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
	load_pcx_file_1(game, 2, "hoofd.pcx", first ? 0x1f : 0x19);
	handle_wipe(2, WIPE_PIXELFADE, 600);
	blt_scr_2_scr(2, 0, 0, 0, SCREEN_W, SCREEN_H, 0); /* do_screen_update */
	mm_display_info();
	mm_animate_buttons();
	mm_animate_map(slot_list[slot_check_index(active_slot)].stage);
	activate_form(mm_form);
	xvga_show();
}

/* The player's series so far, as a high-score entry. */
static int add_slot_hiscore(void)
{
	slot_t *s = &slot_list[slot_check_index(active_slot)];
	return add_new_hiscore(s->name, slot_count_total_tijd(active_slot, s->stage), s->stage - 1,
		s->money);
}

/* "Score": the lists, with the player's series so far added for this viewing only. */
static void mm_handle_tmp_hiscore(void)
{
	save_tmp_scoredbase("SCORE.TMP");
	if (slot_list[slot_check_index(active_slot)].stage - 1 >= 0)
		add_slot_hiscore();
	menu_score(game, slot_list[slot_check_index(active_slot)].stage - 1);
	load_tmp_scoredbase("SCORE.TMP");
	file_delete("SCORE.TMP");
}

/* "Opnieuw": the series starts over. After a stage or more, the result goes into the
 * high scores first, and the list is shown when it made the top ten. */
static void mm_handle_opnieuw_spel(void)
{
	if (slot_list[slot_check_index(active_slot)].stage - 1 < 0) {
		reset_slot(active_slot);
		mm_display_info();
	}
	else {
		mm_close_screen(0);
		add_slot_hiscore();
		int rank = get_last_entered_hiscore();
		if (rank >= 0 && rank < 10)
			menu_score(game, slot_list[slot_check_index(active_slot)].stage - 1);
		reset_slot(active_slot);
		mm_open_screen_1(0);
	}
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
			wait_form_key_released();
			snd_wait(&click);
			snd_set_volume(&mm_tune_buffer, -8000);
			play_help(mm_help, 1);
			snd_set_volume(&mm_tune_buffer, snd_volume_for_level(opties.music_volume));
			break;
		case MM_EXIT:
			snd_play(&click, "click1.wav");
			running = 0;
			break;
		case MM_START:
			snd_play(&click, "click1.wav");
			wait_form_key_released();
			snd_wait(&click);
			result = MENU_START_RACE;
			running = 0;
			break;
		case MM_RESTART:
			snd_play(&click, "click1.wav");
			wait_form_key_released();
			snd_wait(&click);
			snd_set_volume(&mm_tune_buffer, snd_volume_for_level(opties.music_volume / 2));
			snd_play(&click, "MM_07.wav");
			if (req_vraag("De huidige race wordt afgelast. Alle coureurs worden geacht opnieuw "
					"te beginnen.\nWeet je zeker dat je opnieuw wil beginnen?")) {
				snd_stop(&click);
				mm_handle_opnieuw_spel();
			}
			snd_set_volume(&mm_tune_buffer, snd_volume_for_level(opties.music_volume));
			snd_stop(&click);
			activate_form(mm_form);
			break;
		case MM_NEW_PLAYER:
			snd_play(&click, "click1.wav");
			wait_form_key_released();
			snd_wait(&click);
			snd_set_volume(&mm_tune_buffer, snd_volume_for_level(opties.music_volume / 2));
			snd_play(&click, "MM_06.wav");
			if (req_vraag("Weet je zeker dat je de huidige race wil onderbreken?")) {
				snd_stop(&click);
				snd_set_volume(&mm_tune_buffer, snd_volume_for_level(opties.music_volume));
				mm_close_screen(0);
				menu_nieuwesplr(game, &mm_tune_buffer);
				mm_open_screen_1(0);
				break;
			}
			snd_stop(&click);
			snd_set_volume(&mm_tune_buffer, snd_volume_for_level(opties.music_volume));
			activate_form(mm_form);
			break;
		case MM_OPTIONS: {
			snd_play(&click, "click1.wav");
			mm_close_screen(0);
			snd_wait(&click);
			int stage = slot_list[slot_check_index(active_slot)].stage;
			menu_opties(game, &mm_tune_buffer, mm_tune[stage]);
			update_active_slot_optie();
			mm_open_screen_1(0);
			break;
		}
		case MM_CONTROLS:
			snd_play(&click, "click1.wav");
			mm_close_screen(0);
			snd_wait(&click);
			menu_bedien(game, &mm_tune_buffer);
			update_active_slot_bediening();
			mm_open_screen_1(0);
			break;
		case MM_HISCORE:
			snd_play(&click, "click1.wav");
			mm_close_screen(0);
			snd_wait(&click);
			mm_handle_tmp_hiscore();
			mm_open_screen_1(0);
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
