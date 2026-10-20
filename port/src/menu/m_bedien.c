/* The controls menu (m_bedien.cpp): keyboard, joypad or steering wheel, and the keys
 * for left, right, gas and brake. */
#include "menu/m_bedien.h"

#include <stdio.h>

#include "gfx/screen.h"
#include "gfx/wipes.h"
#include "menu/forms.h"
#include "menu/help.h"
#include "menu/menu_data.h"
#include "menu/menu_util.h"
#include "menu/mysound.h"
#include "menu/optie.h"
#include "menu/requesters.h"
#include "menu/txttool.h"
#include "platform/sys.h"
#include "xvga/fileio.h"
#include "xvga/font.h"

/* Controls of mc_form / mc_form_joy, by index. */
enum {
	MC_OK = 0,
	MC_CANCEL = 1,
	MC_HELP = 2,
	MC_KEYBOARD = 3,
	MC_JOYPAD = 4,
	MC_WHEEL = 5,
	MC_CONFIGURE = 6, /* "Instellen", keyboard only */
};

#define KEY_COUNT 4 /* left, right, gas, brake */

static int *music;

static int mc_af_image(int anim, int frame)
{
	return anims[anim].frames[frame].image;
}

/* The text bar on the left (anim 0x27 frame 7: fully open). */
static void mc_clear_text_bar(void)
{
	set_mem_image(0, mc_af_image(0x27, 7), 0x17, 0x79, MI_TOPLEFT);
	xvga_show();
}

static void mc_print_text_bar(const char *fmt, const char *a, const char *b)
{
	char text[256];
	snprintf(text, sizeof text, fmt, a, b);
	set_mem_image(1, mc_af_image(0x27, 7), 0x17, 0x79, MI_TOPLEFT);
	xvga_select_font(3);
	xvga_set_font_colors(3, 0, 0x5a32ff);
	win_printwrapped(1, 0x2b, 0x8d, 200, "%s", text);
	blt_scr_2_scr(1, 0, 0x17, 0x79, 0x10e, 0xe2, 0);
	xvga_show();
}

/* Index of a key in key_list, 0 (no key) when it is not there. */
static int scan_2_ckey(int vk)
{
	for (int i = 0; i < (int)(sizeof key_list / sizeof key_list[0]); i++) {
		if (key_list[i].vk == vk)
			return i;
	}
	return 0;
}

static int *control_key(int i)
{
	int *keys[KEY_COUNT] = { &control.key_left, &control.key_right, &control.key_gas,
		&control.key_brake };
	return keys[i];
}

static void load_ckey(void)
{
	for (int i = 0; i < KEY_COUNT; i++)
		config_key[i].key = *control_key(i);
}

static void save_ckey(void)
{
	for (int i = 0; i < KEY_COUNT; i++)
		*control_key(i) = config_key[i].key;
}

static void print_keys(void)
{
	xvga_select_font(3);
	for (int i = 0; i < KEY_COUNT; i++) {
		set_mem_image(0, mc_af_image(0x34, 0), config_key[i].x - 8, config_key[i].y - 8,
			MI_TOPLEFT);
		xvga_set_font_colors(3, 0, 0xfaf0ff);
		win_print_xy(0, config_key[i].x, config_key[i].y, "%8s",
			key_list[scan_2_ckey(config_key[i].key)].name);
	}
	xvga_show();
}

static void clear_keys(void)
{
	mc_clear_text_bar();
	for (int i = 0; i < KEY_COUNT; i++)
		set_mem_image(0, mc_af_image(0x34, 0), config_key[i].x - 8, config_key[i].y - 8,
			MI_TOPLEFT);
	xvga_show();
}

static void clear_all_ckey(void)
{
	for (int i = 0; i < KEY_COUNT; i++)
		config_key[i].key = -1;
}

/* Which setting already uses this key, or -1. */
static int ckey_new_define(int vk)
{
	for (int i = 0; i < KEY_COUNT; i++) {
		if (config_key[i].key == vk)
			return i;
	}
	return -1;
}

/* Waits for Escape or one of the keys in key_list, and its release. */
static int mb_read_key(void)
{
	sys_input in = { 0 };
	for (;;) {
		sys_poll(&in);
		xvga_show();
		if (in.quit || keyispressed(0x1b)) {
			while (keyispressed(0x1b))
				sys_sleep(5);
			return 0x1b;
		}
		for (int i = 1; i < (int)(sizeof key_list / sizeof key_list[0]); i++) {
			if (keyispressed(key_list[i].vk)) {
				while (keyispressed(key_list[i].vk)) {
					sys_poll(&in);
					sys_sleep(5);
				}
				return key_list[i].vk;
			}
		}
		sys_sleep(5);
	}
}

static void animate_textbox(int from, int to, int step)
{
	for (int frame = from; frame != to + step; frame += step) {
		uint32_t t = sys_ticks();
		set_mem_image(0, mc_af_image(0x27, frame), 0x17, 0x79, MI_TOPLEFT);
		xvga_show();
		sys_input in = { 0 };
		do {
			sys_poll(&in);
			sys_sleep(1);
		} while (sys_ticks() - t < 0x21);
	}
}

/* "Instellen": asks for the four keys one by one. */
static int config_keys(void)
{
	int voice = -1, click = -1;
	animate_textbox(0, 7, 1);
	snd_set_volume(music, snd_volume_for_level(opties.music_volume / 2));
	clear_all_ckey();
	for (int i = 0; i < KEY_COUNT;) {
		print_keys();
		snd_play(&voice, mc_instel_wav[i]);
		mc_print_text_bar("%s %s", "Kies nu een toets voor:\n", config_key[i].label);
		int vk = mb_read_key();
		if (vk == 0x1b) {
			snd_stop(&click);
			snd_play(&click, "click1.wav");
			load_ckey();
			print_keys();
			animate_textbox(7, 0, -1);
			snd_wait(&click);
			snd_set_volume(music, snd_volume_for_level(opties.music_volume));
			return 0;
		}
		snd_play(&click, "click3.wav");
		if (ckey_new_define(vk) >= 0) {
			clear_keys();
			snd_play(&voice, "MC_09.wav");
			req_warning("Deze toets, <%s>, is eerder gedefinieerd. Kies een andere toets.",
				key_list[scan_2_ckey(vk)].name);
			snd_stop(&voice);
			continue;
		}
		config_key[i++].key = vk;
		print_keys();
	}
	snd_play(&voice, "MC_10.wav");
	mc_print_text_bar("%s%s", "De toetsen zijn ingesteld.", "");
	snd_wait(&voice);
	animate_textbox(7, 0, -1);
	snd_set_volume(music, snd_volume_for_level(opties.music_volume));
	return 1;
}

/* The picture of the chosen device (anim 0x33: keyboard, joypad, wheel, keyboard only). */
static void display_device(int frame)
{
	set_mem_image(2, mc_af_image(0x33, frame), 0x132, 0x7d, MI_TOPLEFT);
	set_mem_image(0, mc_af_image(0x33, frame), 0x132, 0x7d, MI_TOPLEFT);
	xvga_show();
}

static void display_keys(void)
{
	display_device(available_joy ? 0 : 3);
	print_keys();
}

static void mc_display_controls(void)
{
	if (!available_joy)
		control.control_type = 0;
	if (control.control_type == 0) {
		display_keys();
		mc_form[4].state = mc_form[5].state = available_joy ? 3 : 0;
	}
	else {
		display_device(control.control_type == 1 ? 1 : 2);
		mc_form_joy[3].state = mc_form_joy[4].state = mc_form_joy[5].state = 3;
	}
}

static void mc_animate_buttons(void)
{
	for (int step = 0; step < 8; step++) {
		uint32_t t = sys_ticks();
		set_mem_image(0, mc_af_image(0x2a, mc_entree_anim_tabel[0][step]), 0, 0x14b, MI_TOPLEFT);
		set_mem_image(0, mc_af_image(0x2b, mc_entree_anim_tabel[1][step]), 0, 0x17e, MI_TOPLEFT);
		xvga_show();
		sys_input in = { 0 };
		do {
			sys_poll(&in);
			sys_sleep(1);
		} while (sys_ticks() - t < 0x21);
	}
}

static void mc_open_screen(game_t *g)
{
	load_pcx_file_1(g, 2, "bedien.pcx", 0x1f);
	handle_wipe(2, WIPE_PIXELFADE, 600);
	blt_scr_2_scr(2, 1, 0, 0, SCREEN_W, SCREEN_H, 0); /* do_back_screen_update */
	load_ckey();
	mc_display_controls();
	mc_animate_buttons();
	activate_form(control.control_type == 0 ? mc_form : mc_form_joy);
	focus_control(0);
}

static void switch_device(int type)
{
	control.control_type = type;
	destroy_form();
	if (type == 0) {
		display_keys();
		activate_form(mc_form);
	}
	else {
		display_device(type);
		activate_form(mc_form_joy);
	}
	focus_control(0);
}

int menu_bedien(game_t *g, int *music_handle)
{
	int snd = -1, running = 1, cancel = 0;
	music = music_handle;
	bedien_t saved = control; /* save_tmp_bedien: BEDIEN.TMP */
	file_write(&control, sizeof control, "BEDIEN.TMP");
	mc_open_screen(g);
	sys_input in = { 0 };
	while (running) {
		sys_poll(&in);
		int r = handle_form(&in);
		switch (r) {
		case MC_OK:
			snd_play(&snd, "click1.wav");
			running = 0;
			snd_wait(&snd);
			break;
		case FORM_ESCAPE:
		case FORM_QUIT:
			focus_control(MC_CANCEL);
			/* fall through */
		case MC_CANCEL:
			snd_play(&snd, "click1.wav");
			running = 0;
			cancel = 1;
			snd_wait(&snd);
			break;
		case MC_HELP:
			snd_play(&snd, "click1.wav");
			wait_form_key_released();
			snd_wait(&snd);
			snd_set_volume(music, -8000);
			play_help(available_joy ? mc_help : mc_help2, 1);
			snd_set_volume(music, snd_volume_for_level(opties.music_volume));
			break;
		case MC_KEYBOARD:
			snd_play(&snd, "click2.wav");
			wait_form_key_released();
			if (control.control_type != 0)
				switch_device(0);
			snd_wait(&snd);
			break;
		case MC_JOYPAD:
		case MC_WHEEL:
			if (!available_joy)
				break;
			snd_play(&snd, "click2.wav");
			wait_form_key_released();
			if (control.control_type != r - MC_KEYBOARD)
				switch_device(r - MC_KEYBOARD);
			snd_wait(&snd);
			break;
		case MC_CONFIGURE:
			if (control.control_type != 0)
				break;
			snd_play(&snd, "click1.wav");
			wait_form_key_released();
			destroy_form();
			config_keys();
			display_keys();
			mc_display_controls();
			activate_form(mc_form);
			focus_control(MC_CONFIGURE);
			break;
		case FORM_UP:
		case FORM_DOWN:
			snd_play(&snd, "click3.wav");
			focus_control(get_control_focus() == MC_OK ? MC_CANCEL : MC_OK);
			snd_wait(&snd);
			break;
		default:
			break;
		}
		xvga_show();
		sys_sleep(10);
	}
	wait_form_key_released();
	destroy_form();
	save_ckey();
	if (cancel)
		control = saved; /* load_tmp_bedien */
	file_delete("BEDIEN.TMP");
	save_bedien();
	return 0;
}
