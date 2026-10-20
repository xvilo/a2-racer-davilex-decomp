/* The start screen: pick or create a player (m_start.cpp). */
#include "menu/m_start.h"

#include <stdio.h>
#include <string.h>

#include "gfx/screen.h"
#include "gfx/wipes.h"
#include "menu/forms.h"
#include "menu/help.h"
#include "menu/m_main.h"
#include "menu/menu_data.h"
#include "menu/menu_util.h"
#include "menu/mysound.h"
#include "menu/optie.h"
#include "menu/requesters.h"
#include "menu/slot.h"
#include "menu/txttool.h"
#include "platform/sys.h"
#include "xvga/font.h"
#include "xvga/sound.h"

/* Controls of mst_form, by index; 4..13 are the ten slot rows. */
enum {
	MST_OK = 0,
	MST_DELETE = 1,  /* "Verwijder" */
	MST_HELP = 2,
	MST_EXIT = 3,    /* "Einde" */
	MST_SLOT0 = 4,
};

static const char *const ask_name = "Vul je naam in en wordt lid van de A2 Racing Club!";

/* One slot row: the name, the stage, the money and the total time. */
static void mst_draw_slot_text(int slot)
{
	int x = mst_slot_list[slot].x, y = mst_slot_list[slot].y;
	win_bprintf(1, x + 10, y, 0x80, "%-16s", slot_naam_string(slot));
	win_bprintf(1, x + 0x94, y, 0xfa, "%-16s", slot_level_string(slot));
	win_bprintf(1, x + 0xfe, y, 0xfa, "%9d", slot_list[slot].money);
	win_bprintf(1, x + 0x189, y, 0x80, "%-6s", slot_tijd_string(slot));
}

static void mst_draw_slot_info(int slot)
{
	set_mem_image(1, anims[5].frames[0].image, mst_slot_list[slot].x, mst_slot_list[slot].y,
		MI_TOPLEFT);
	xvga_select_font(2);
	xvga_set_font_colors(2, 0xafaff, 0xafaff);
	mst_draw_slot_text(slot);
}

/* The selected row has a highlighted bar and white text. */
static void mst_draw_selected_slot_info(int slot)
{
	set_mem_image(1, anims[5].frames[1].image, mst_slot_list[slot].x, mst_slot_list[slot].y,
		MI_TOPLEFT);
	xvga_select_font(2);
	xvga_set_font_colors(2, 0xfafafa, 0xfafafa);
	mst_draw_slot_text(slot);
}

static void mst_reselect_slot(int old_slot, int new_slot)
{
	blt_scr_2_scr(0, 1, 0x42, 0x8d, 0x23b, 0x17d, 0);
	mst_draw_slot_info(old_slot);
	mst_draw_selected_slot_info(new_slot);
	blt_scr_2_scr(1, 0, 0x42, 0x8d, 0x23b, 0x17d, 0);
	xvga_show();
}

static void mst_print_slot(int selected)
{
	blt_scr_2_scr(2, 1, 0x42, 0x8d, 0x23b, 0x17d, 0);
	xvga_select_font(2);
	for (int i = 0; i < SLOT_COUNT; i++)
		mst_draw_slot_info(i);
	mst_draw_selected_slot_info(selected);
	blt_scr_2_scr(1, 0, 0x42, 0x8d, 0x23b, 0x17d, 0);
	xvga_show();
}

/* The A2 logo with its jingle, the first time the game is started. */
static void mst_handle_intro(game_t *g, int show)
{
	if (!show)
		return;
	load_pcx_file_1(g, 2, "LOGO256.PCX", 0x1f);
	clear_screen(0, 0);
	handle_wipe(2, WIPE_PIXELFADE, 600);
	int snd = -1;
	snd_play(&snd, "algintro.wav");
	sys_input in = { 0 };
	while (sound_buffer_busy(snd) && !keyispressed(0x20) && !keyispressed(0x1b)) {
		sys_poll(&in);
		if (in.quit)
			break;
		xvga_show();
		sys_sleep(10);
	}
	snd_stop(&snd);
	clear_screen(2, 0);
	handle_wipe(2, WIPE_PIXELFADE, 600);
}

static void mst_open_screen(game_t *g, int selected)
{
	load_pcx_file_1(g, 2, "NSPEL_1.PCX", 0x1f);
	clear_screen(0, 0);
	handle_wipe(2, WIPE_PIXELFADE, 600);
	mst_print_slot(selected);
	activate_form(mst_form);
}

static void mst_close_screen(void)
{
	wait_form_key_released();
	destroy_form();
	clear_screen(2, 0);
	handle_wipe(2, WIPE_PIXELFADE, 600);
}

int menu_start(game_t *g, int first)
{
	int snd = -1, running = 1, quit = 0;
	char name[32];
	int slot = 0;
	if (first)
		slot = slot_list[slot_check_index(get_last_saved_slot())].used ? 0 : 1;
	mst_handle_intro(g, slot);
	slot = get_last_saved_slot();
	mst_open_screen(g, slot);
	if (first)
		snd_play(&snd, "MNS_00.wav");

	/* No players yet: ask for a name straight away. */
	if (!slot_list[slot_check_index(get_last_saved_slot())].used) {
		snd_wait(&snd);
		snd_play(&snd, "MNS_01.wav");
		if (req_input(name, sizeof name, "%s", ask_name)) {
			snd_stop(&snd);
			create_slot(slot, name);
			mst_print_slot(slot);
			set_active_slot(slot);
			mst_close_screen();
			save_slotdbase();
			return 1;
		}
		snd_stop(&snd);
	}
	activate_form(mst_form);
	sys_input in = { 0 };
	while (running) {
		sys_poll(&in);
		int r = handle_form(&in);
		if (r >= MST_SLOT0 && r < MST_SLOT0 + SLOT_COUNT) {
			int picked = r - MST_SLOT0;
			if (picked == slot) {
				/* Clicking the selected row again is OK. */
				focus_control(MST_OK);
				r = MST_OK;
			}
			else {
				snd_play(&snd, "click3.wav");
				mst_reselect_slot(slot, picked);
				slot = picked;
				snd_wait(&snd);
				continue;
			}
		}
		switch (r) {
		case MST_OK:
			snd_play(&snd, "click1.wav");
			if (!slot_list[slot_check_index(slot)].used) {
				snd_wait(&snd);
				snd_play(&snd, "MNS_01.wav");
				if (!req_input(name, sizeof name, "%s", ask_name)) {
					snd_stop(&snd);
					activate_form(mst_form);
					break;
				}
				snd_stop(&snd);
				create_slot(slot, name);
				mst_print_slot(slot);
			}
			set_active_slot(slot);
			running = 0;
			snd_wait(&snd);
			break;
		case MST_DELETE:
			if (slot_list[slot_check_index(slot)].used) {
				snd_play(&snd, "click1.wav");
				snd_wait(&snd);
				snd_play(&snd, "MNS_02.wav");
				if (req_vraag("Wil je deze speler '%s' verwijderen?", slot_naam_string(slot)))
					clear_slot(slot);
				snd_stop(&snd);
				slot = get_last_saved_slot();
				mst_print_slot(slot);
			}
			activate_form(mst_form);
			break;
		case MST_HELP:
			snd_play(&snd, "click1.wav");
			wait_form_key_released();
			snd_wait(&snd);
			play_help(mst_help, 1);
			break;
		case FORM_ESCAPE:
			focus_control(MST_EXIT);
			/* fall through */
		case MST_EXIT:
		case FORM_QUIT:
			quit = 1;
			snd_play(&snd, "click1.wav");
			running = 0;
			snd_wait(&snd);
			break;
		case FORM_UP:
		case FORM_DOWN: {
			int next = r == FORM_UP ? (slot + 9) % 10 : (slot + 1) % 10;
			snd_play(&snd, "click3.wav");
			mst_reselect_slot(slot, next);
			snd_wait(&snd);
			slot = next;
			break;
		}
		default:
			break;
		}
		xvga_show();
		sys_sleep(10);
	}
	mst_close_screen();
	save_slotdbase();
	return !quit;
}
