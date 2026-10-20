/* The options menu (m_opties.cpp): display mode, sound, horizon, detail, dashboard and
 * volumes. menu_opties's switch is taken from the machine code (Ghidra could not follow
 * its jump table). */
#include "menu/m_opties.h"

#include <stdio.h>

#include "gfx/screen.h"
#include "gfx/wipes.h"
#include "menu/forms.h"
#include "menu/help.h"
#include "menu/menu_data.h"
#include "menu/menu_util.h"
#include "menu/mysound.h"
#include "menu/optie.h"
#include "platform/sys.h"
#include "xvga/fileio.h"

/* Controls of mo_form_*, by index (the same in all three forms). */
enum {
	MO_OK = 0,
	MO_CANCEL = 1, /* "Annuleren" */
	MO_HELP = 2,
	MO_DISPLAY_3D = 3,
	MO_DISPLAY_320 = 4,
	MO_DISPLAY_640 = 5,
	MO_HORIZON_ON = 6,
	MO_HORIZON_OFF = 7,
	MO_SOUND_EFFECTS = 8,
	MO_SOUND_MUSIC = 9,
	MO_SOUND_ALL = 10,
	MO_SOUND_OFF = 11,
	MO_DETAIL_ON = 12,
	MO_DETAIL_OFF = 13,
	MO_DASHBOARD_ON = 14,
	MO_DASHBOARD_OFF = 15,
	MO_MUSIC_DOWN = 16,
	MO_MUSIC_UP = 17,
	MO_EFFECTS_DOWN = 18,
	MO_EFFECTS_UP = 19,
};

static int mo_af_image(int anim, int frame)
{
	return anims[anim].frames[frame].image;
}

/* The form matching the display mode: each shows only the settings that apply. */
static void mc_active_form(void)
{
	destroy_form();
	if (opties.display_mode == 0)
		activate_form(mo_form_3D);
	else if (opties.display_mode == 2)
		activate_form(mo_form_640);
	else
		activate_form(mo_form_320);
	focus_control(MO_OK);
}

/* Radio lights: anim 0x3b frame 1 when the setting has that value. */
static void radio(int x, int y, int on)
{
	set_mem_image(1, mo_af_image(0x3b, on ? 1 : 0), x, y, MI_TOPLEFT);
}

static void mc_display_geluid(void)
{
	set_mem_image(1, mo_af_image(0x43, 0), 0x10c, 0xf5, MI_TOPLEFT);
	set_mem_image(1, mo_af_image(0x43, 1), 0x15d, 0xf5, MI_TOPLEFT);
	set_mem_image(1, mo_af_image(0x43, 2), 0x15d, 0x10a, MI_TOPLEFT);
	set_mem_image(1, mo_af_image(0x43, 3), 0x15d, 0x11f, MI_TOPLEFT);
	set_mem_image(1, mo_af_image(0x43, 4), 0x15d, 0x134, MI_TOPLEFT);
	radio(0x14a, 0xf6, opties.sound == SOUND_EFFECTS_ONLY);
	radio(0x14a, 0x10b, opties.sound == SOUND_MUSIC_ONLY);
	radio(0x14a, 0x120, opties.sound == SOUND_ALL);
	radio(0x14a, 0x135, opties.sound == SOUND_OFF);
}

static void mc_display_horizon(void)
{
	set_mem_image(1, mo_af_image(0x45, 0), 0x101, 0xa4, MI_TOPLEFT);
	set_mem_image(1, mo_af_image(0x45, 1), 0x15d, 0xa4, MI_TOPLEFT);
	set_mem_image(1, mo_af_image(0x45, 2), 0x15d, 0xb8, MI_TOPLEFT);
	radio(0x14a, 0xa5, opties.horizon == 1);
	radio(0x14a, 0xb9, opties.horizon == 0);
}

static void mc_display_detail(void)
{
	set_mem_image(1, mo_af_image(0x44, 0), 0x1dc, 0x164, MI_TOPLEFT);
	set_mem_image(1, mo_af_image(0x44, 1), 0x228, 0x164, MI_TOPLEFT);
	set_mem_image(1, mo_af_image(0x44, 2), 0x228, 0x179, MI_TOPLEFT);
	radio(0x215, 0x165, opties.detail == 1);
	radio(0x215, 0x17a, opties.detail == 0);
}

static void mc_display_dashbord(void)
{
	set_mem_image(1, mo_af_image(0x46, 0), 0xe9, 0x164, MI_TOPLEFT);
	set_mem_image(1, mo_af_image(0x46, 1), 0x15d, 0x164, MI_TOPLEFT);
	set_mem_image(1, mo_af_image(0x46, 2), 0x15d, 0x179, MI_TOPLEFT);
	radio(0x14a, 0x165, opties.dashboard == 1);
	radio(0x14a, 0x17a, opties.dashboard == 0);
}

/* The settings that apply to the display mode. */
static void mc_display_inhoud(void)
{
	blt_scr_2_scr(2, 1, 0xe6, 0x96, 600, 0x19a, 0);
	mc_display_geluid();
	if (opties.display_mode == 0) {
		mc_display_dashbord();
		mc_display_horizon();
	}
	else if (opties.display_mode < 2) {
		mc_display_detail();
	}
	else if (opties.display_mode == 2) {
		mc_display_horizon();
		mc_display_detail();
		mc_display_dashbord();
	}
	blt_scr_2_scr(1, 0, 0xe6, 0x96, 600, 0x19a, 0);
	xvga_show();
}

/* The volume lights on the left: lit up to the volume, off for a muted part. */
static void mc_display_snd_setting(void)
{
	set_mem_image(1, mo_af_image(0x36, opties.sound != SOUND_OFF), 0, 0x78, MI_TOPLEFT);
	if (opties.sound != SOUND_OFF) {
		for (int i = 0, x = 0x2e; i < 5; i++, x += 0x1a) {
			int anim = opties.music_volume < i || opties.sound == SOUND_EFFECTS_ONLY ? 0x38 : 0x37;
			set_mem_image(1, mo_af_image(anim, i), x, 0x9f, MI_TOPLEFT);
			anim = opties.effects_volume < i || opties.sound == SOUND_MUSIC_ONLY ? 0x38 : 0x37;
			set_mem_image(1, mo_af_image(anim, i), x, 0xe5, MI_TOPLEFT);
		}
	}
	blt_scr_2_scr(1, 0, 0, 0x78, 0xcb, 0xf8, 0);
	xvga_show();
}

/* The tablet with the three display modes. */
static void mc_display_tableau(void)
{
	int frame = opties.display_mode == 0 ? 0 : opties.display_mode < 2 ? 1 : 2;
	set_mem_image(2, mo_af_image(0x35, frame), 0, 0x70, MI_TOPLEFT);
	set_mem_image(0, mo_af_image(0x35, frame), 0, 0x70, MI_TOPLEFT);
	mc_display_inhoud();
	mc_display_snd_setting();
}

static void mo_animate_buttons(void)
{
	for (int step = 0; step < 8; step++) {
		uint32_t t = sys_ticks();
		set_mem_image(0, mo_af_image(0x3f, mo_entree_anim_tabel[0][step]), 0, 0x14b, MI_TOPLEFT);
		set_mem_image(0, mo_af_image(0x40, mo_entree_anim_tabel[1][step]), 0, 0x17e, MI_TOPLEFT);
		xvga_show();
		sys_input in = { 0 };
		do {
			sys_poll(&in);
			sys_sleep(1);
		} while (sys_ticks() - t < 0x21);
	}
}

static void mo_open_screen(game_t *g)
{
	load_pcx_file_1(g, 2, "Opties.pcx", 0x1f);
	handle_wipe(2, WIPE_PIXELFADE, 600);
	mc_display_tableau();
	mc_display_snd_setting();
	mo_animate_buttons();
	mc_active_form();
	focus_control(MO_OK);
}

static void mo_close_screen(void)
{
	wait_form_key_released();
	destroy_form();
}

/* Sets a setting if it changed; returns whether it did. */
static int set_option(int16_t *field, int value)
{
	if (*field == value)
		return 0;
	*field = (int16_t)value;
	return 1;
}

int menu_opties(game_t *g, int *music, const char *tune)
{
	int snd = -1, running = 1, cancel = 0;
	/* save_tmp_optie: Annuleren puts these back. */
	opties_t saved = opties;
	file_write(&opties, sizeof opties, "Opties.TMP");
	mo_open_screen(g);
	sys_input in = { 0 };
	while (running) {
		sys_poll(&in);
		int r = handle_form(&in);
		int sound_was = opties.sound;
		switch (r) {
		case FORM_UP:
		case FORM_DOWN:
			/* The arrow keys move between OK and Annuleren. */
			snd_play(&snd, "click3.wav");
			focus_control(get_control_focus() == MO_OK ? MO_CANCEL : MO_OK);
			snd_wait(&snd);
			break;
		case FORM_ESCAPE:
		case FORM_QUIT:
			focus_control(MO_CANCEL);
			/* fall through */
		case MO_CANCEL:
			cancel = 1;
			/* fall through */
		case MO_OK:
			snd_play(&snd, "click1.wav");
			running = 0;
			snd_wait(&snd);
			break;
		case MO_HELP:
			snd_play(&snd, "click1.wav");
			wait_form_key_released();
			snd_set_volume(music, -8000);
			play_help(mo_help, 1);
			snd_set_volume(music, snd_volume_for_level(opties.music_volume));
			snd_wait(&snd);
			break;
		case MO_DISPLAY_3D:
			snd_play(&snd, "click3.wav");
			if (set_option(&opties.display_mode, 0)) {
				mc_display_tableau();
				mc_active_form();
			}
			break;
		case MO_DISPLAY_320:
			snd_play(&snd, "click3.wav");
			if (opties.display_mode != 1) {
				opties.horizon = 0;
				opties.dashboard = 0;
				opties.display_mode = 1;
				mc_display_tableau();
				mc_active_form();
			}
			break;
		case MO_DISPLAY_640:
			snd_play(&snd, "click3.wav");
			if (set_option(&opties.display_mode, 2)) {
				mc_display_tableau();
				mc_active_form();
			}
			break;
		case MO_HORIZON_ON:
		case MO_HORIZON_OFF:
			snd_play(&snd, "click3.wav");
			if (set_option(&opties.horizon, r == MO_HORIZON_ON))
				mc_display_inhoud();
			break;
		case MO_SOUND_EFFECTS:
		case MO_SOUND_MUSIC:
		case MO_SOUND_ALL:
		case MO_SOUND_OFF: {
			static const int value[4] = { SOUND_EFFECTS_ONLY, SOUND_MUSIC_ONLY, SOUND_ALL,
				SOUND_OFF };
			snd_play(&snd, "click3.wav");
			if (set_option(&opties.sound, value[r - MO_SOUND_EFFECTS])) {
				mc_display_snd_setting();
				mc_display_inhoud();
				if (opties.sound == SOUND_EFFECTS_ONLY || opties.sound == SOUND_OFF)
					snd_stop(music);
				else
					snd_play_looping(music, tune);
			}
			(void)sound_was;
			break;
		}
		case MO_DETAIL_ON:
		case MO_DETAIL_OFF:
			snd_play(&snd, "click3.wav");
			if ((opties.display_mode == 1 || opties.display_mode == 2) &&
				set_option(&opties.detail, r == MO_DETAIL_ON))
				mc_display_inhoud();
			break;
		case MO_DASHBOARD_ON:
		case MO_DASHBOARD_OFF:
			snd_play(&snd, "click3.wav");
			if ((opties.display_mode == 0 || opties.display_mode == 2) &&
				set_option(&opties.dashboard, r == MO_DASHBOARD_ON))
				mc_display_inhoud();
			break;
		case MO_MUSIC_DOWN:
		case MO_MUSIC_UP:
			snd_play(&snd, "click3.wav");
			if (opties.sound == SOUND_ALL || opties.sound == SOUND_MUSIC_ONLY) {
				if (r == MO_MUSIC_DOWN && opties.music_volume > 0)
					opties.music_volume--;
				if (r == MO_MUSIC_UP && opties.music_volume < 4)
					opties.music_volume++;
				snd_set_volume(music, snd_volume_for_level(opties.music_volume));
				mc_display_snd_setting();
			}
			break;
		case MO_EFFECTS_DOWN:
		case MO_EFFECTS_UP:
			snd_play(&snd, "click3.wav");
			if (opties.sound == SOUND_ALL || opties.sound == SOUND_EFFECTS_ONLY) {
				if (r == MO_EFFECTS_DOWN && opties.effects_volume > 0)
					opties.effects_volume--;
				if (r == MO_EFFECTS_UP && opties.effects_volume < 4)
					opties.effects_volume++;
				snd_set_volume(&snd, snd_volume_for_level(opties.effects_volume));
				mc_display_snd_setting();
			}
			break;
		default:
			break;
		}
		xvga_show();
		sys_sleep(10);
	}
	mo_close_screen();
	if (cancel) {
		/* load_tmp_optie, then the music follows the restored setting. */
		int changed = opties.sound;
		opties = saved;
		int restored = opties.sound;
		if ((restored == SOUND_ALL || restored == SOUND_MUSIC_ONLY) &&
			(changed == SOUND_EFFECTS_ONLY || changed == SOUND_OFF))
			snd_play_looping(music, tune);
		if ((restored == SOUND_EFFECTS_ONLY || restored == SOUND_OFF) &&
			(changed == SOUND_ALL || changed == SOUND_MUSIC_ONLY))
			snd_stop(music);
		snd_set_volume(music, snd_volume_for_level(opties.music_volume));
	}
	file_delete("Opties.TMP");
	save_optie();
	return 0;
}
