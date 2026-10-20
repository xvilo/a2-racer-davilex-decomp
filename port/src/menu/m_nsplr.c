/* The new-player screen (m_nsplr.cpp): from the main menu, switch to another player or
 * create one. Cancelling puts the slots back as they were (OPSTART.TMP). */
#include "menu/m_nsplr.h"

#include <stdio.h>
#include <string.h>

#include "gfx/screen.h"
#include "gfx/wipes.h"
#include "menu/forms.h"
#include "menu/help.h"
#include "menu/menu_data.h"
#include "menu/menu_util.h"
#include "menu/mysound.h"
#include "menu/optie.h"
#include "menu/requesters.h"
#include "menu/slot.h"
#include "menu/txttool.h"
#include "platform/sys.h"
#include "xvga/fileio.h"
#include "xvga/font.h"

/* Controls of mns_form, by index; 4..13 are the ten slot rows. */
enum {
	MNS_OK = 0,
	MNS_DELETE = 1,
	MNS_CANCEL = 2,
	MNS_HELP = 3,
	MNS_SLOT0 = 4,
};

/* One row, drawn into the buffer screen: plain (anim 0 frame 0) or selected (frame 1). */
static void mns_draw_row(int slot, int selected)
{
	int x = mns_slot_list[slot].x, y = mns_slot_list[slot].y;
	set_mem_image(2, anims[0].frames[selected ? 1 : 0].image, x, y, MI_TOPLEFT);
	xvga_select_font(2);
	uint32_t c = selected ? 0xfafafa : 0xafaff;
	xvga_set_font_colors(2, c, c);
	win_bprintf(2, x + 10, y, 0x80, "%-16s", slot_naam_string(slot));
	win_print_xy(2, x + 0x94, y, "%-16s", slot_level_string(slot));
	win_print_xy(2, x + 0xfe, y, "%9d", slot_list[slot].money);
	win_print_xy(2, x + 0x189, y, "%-6s", slot_tijd_string(slot));
}

static void mns_show(void)
{
	blt_scr_2_scr(2, 0, 0x42, 0x8d, 0x23b, 0x17d, 0);
	xvga_show();
}

static void mns_print_slot(int selected)
{
	for (int i = 0; i < SLOT_COUNT; i++)
		mns_draw_row(i, 0);
	mns_draw_row(selected, 1);
	mns_show();
}

static void mns_reselect_slot(int old_slot, int new_slot)
{
	mns_draw_row(old_slot, 0);
	mns_draw_row(new_slot, 1);
	mns_show();
}

int menu_nieuwesplr(game_t *g, int *music)
{
	static const char *const ask_name = "Vul je naam in en wordt lid van de A2 Racing Club!";
	int snd = -1, running = 1, cancel = 0;
	char name[32];
	slot_t saved[SLOT_COUNT];
	memcpy(saved, slot_list, sizeof saved); /* save_tmp_slotdbase: OPSTART.TMP */
	file_write(slot_list, sizeof slot_list, "OPSTART.TMP");
	int slot = get_last_saved_slot();
	load_pcx_file_1(g, 2, "NSPEL_2.PCX", 0x1f);
	handle_wipe(2, WIPE_PIXELFADE, 600);
	mns_print_slot(slot);
	activate_form(mns_form);
	sys_input in = { 0 };
	while (running) {
		sys_poll(&in);
		int r = handle_form(&in);
		if (r >= MNS_SLOT0 && r < MNS_SLOT0 + SLOT_COUNT) {
			int picked = r - MNS_SLOT0;
			if (picked == slot) {
				focus_control(MNS_OK);
				r = MNS_OK;
			}
			else {
				snd_play(&snd, "click3.wav");
				mns_reselect_slot(slot, picked);
				slot = picked;
				snd_wait(&snd);
				continue;
			}
		}
		switch (r) {
		case MNS_OK:
			snd_play(&snd, "click1.wav");
			if (!slot_list[slot_check_index(slot)].used) {
				snd_wait(&snd);
				snd_set_volume(music, snd_volume_for_level(opties.music_volume / 2));
				snd_play(&snd, "MNS_01.wav");
				int ok = req_input(name, sizeof name, "%s", ask_name);
				snd_stop(&snd);
				snd_set_volume(music, snd_volume_for_level(opties.music_volume));
				if (!ok) {
					activate_form(mns_form);
					break;
				}
				create_slot(slot, name);
				mns_print_slot(slot);
			}
			set_active_slot(slot);
			running = 0;
			snd_wait(&snd);
			break;
		case MNS_DELETE:
			if (slot_list[slot_check_index(slot)].used) {
				snd_play(&snd, "click1.wav");
				snd_wait(&snd);
				snd_set_volume(music, snd_volume_for_level(opties.music_volume / 2));
				snd_play(&snd, "MNS_02.wav");
				if (req_vraag("Wil je deze speler '%s' verwijderen?", slot_naam_string(slot)))
					clear_slot(slot);
				snd_stop(&snd);
				snd_set_volume(music, snd_volume_for_level(opties.music_volume));
				slot = get_last_saved_slot();
				mns_print_slot(slot);
			}
			activate_form(mns_form);
			break;
		case FORM_ESCAPE:
		case FORM_QUIT:
			focus_control(MNS_CANCEL);
			/* fall through */
		case MNS_CANCEL:
			cancel = 1;
			snd_play(&snd, "click1.wav");
			running = 0;
			snd_wait(&snd);
			break;
		case MNS_HELP:
			snd_play(&snd, "click1.wav");
			wait_form_key_released();
			snd_wait(&snd);
			snd_set_volume(music, -8000);
			play_help(mns_help, 1);
			snd_set_volume(music, snd_volume_for_level(opties.music_volume));
			break;
		case FORM_UP:
		case FORM_DOWN: {
			int next = r == FORM_UP ? (slot + 9) % 10 : (slot + 1) % 10;
			snd_play(&snd, "click3.wav");
			mns_reselect_slot(slot, next);
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
	wait_form_key_released();
	destroy_form();
	if (cancel)
		memcpy(slot_list, saved, sizeof saved); /* load_tmp_slotdbase */
	file_delete("OPSTART.TMP");
	save_slotdbase();
	return !cancel;
}
