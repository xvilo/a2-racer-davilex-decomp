/* reqvraag.cpp, reqwarn.cpp and reqinput.cpp. */
#include "menu/requesters.h"

#include <stdarg.h>
#include <stdio.h>

#include "gfx/screen.h"
#include "menu/forms.h"
#include "menu/help.h"
#include "menu/menu_data.h"
#include "menu/mysound.h"
#include "menu/txttool.h"
#include "platform/sys.h"
#include "xvga/font.h"
#include "xvga/sound.h"

/* reqv_wait_snd / reqi_wait_snd */
static void req_wait_snd(int *handle)
{
	sys_input in = { 0 };
	while (*handle >= 0 && sound_buffer_busy(*handle)) {
		sys_poll(&in);
		if (in.quit)
			break;
		xvga_show();
		sys_sleep(5);
	}
	*handle = -1;
}

/* Saves the screen, dims it and puts the requester's panel (anim `panel`) on top. */
static void req_open(int panel, const char *text)
{
	blt_scr_2_scr(0, 1, 0, 0, SCREEN_W, SCREEN_H, 0);
	draw_grid(0);
	set_mem_image(0, anims[panel].frames[0].image, 0x97, 0x61, MI_TOPLEFT);
	xvga_select_font(1);
	xvga_set_font_colors(1, 0, 0x1efaff);
	win_printwrapped(0, 0x105, 0x7a, 0xcd, "%s", text);
}

static void req_close(void)
{
	blt_scr_2_scr(1, 0, 0, 0, SCREEN_W, SCREEN_H, 0);
	xvga_show();
	wait_form_key_released();
	destroy_form();
}

int req_vraag(const char *fmt, ...)
{
	char text[256];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(text, sizeof text, fmt, ap);
	va_end(ap);
	int snd = -1, no = 0;
	req_open(0xf, text);
	activate_form(reqv_form);
	focus_control(0);
	sys_input in = { 0 };
	for (int running = 1; running;) {
		sys_poll(&in);
		int r = handle_form(&in);
		if (r == FORM_QUIT || r == 1 || r == FORM_ESCAPE) {
			if (r == FORM_ESCAPE)
				focus_control(1);
			no = 1;
			r = 0;
		}
		if (r == 0) {
			snd_play(&snd, "click2.wav");
			wait_form_key_released();
			req_wait_snd(&snd);
			running = 0;
		}
		xvga_show();
		sys_sleep(10);
	}
	req_close();
	return !no;
}

int req_warning(const char *fmt, ...)
{
	char text[256];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(text, sizeof text, fmt, ap);
	va_end(ap);
	req_open(0x11, text);
	activate_form(reqw_form);
	focus_control(0);
	sys_input in = { 0 };
	for (;;) {
		sys_poll(&in);
		int r = handle_form(&in);
		if (r == FORM_ESCAPE)
			focus_control(0);
		if (r == 0 || r == FORM_ESCAPE || r == FORM_QUIT)
			break;
		xvga_show();
		sys_sleep(10);
	}
	wait_form_key_released();
	req_close();
	return 1;
}

int req_input(char *buf, int size, const char *fmt, ...)
{
	char text[256];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(text, sizeof text, fmt, ap);
	va_end(ap);
	int snd = -1, cancelled = 0;
	req_open(0xc, text);
	xvga_select_font(2);
	xvga_set_font_colors(2, 0, 0xf0fafa);
	xvga_set_font_bkmode(2, 2);
	activate_form(reqi_form);
	focus_control(2);
	sys_input in = { 0 };
	for (int running = 1; running;) {
		sys_poll(&in);
		int r = handle_form(&in);
		if (r == 0) {
			/* OK */
			snd_play(&snd, "click2.wav");
			wait_form_key_released();
			get_edit_box_text(2, buf, size);
			req_wait_snd(&snd);
			running = 0;
		}
		else if (r == FORM_ENTER) {
			/* Enter in the name field counts as OK. */
			focus_control(0);
			snd_play(&snd, "click2.wav");
			get_edit_box_text(2, buf, size);
			while (keyispressed(0x0d))
				sys_sleep(5);
			running = 0;
		}
		else if (r == 1 || r == FORM_ESCAPE || r == FORM_QUIT) {
			if (r == FORM_ESCAPE)
				focus_control(1);
			snd_play(&snd, "click2.wav");
			wait_form_key_released();
			cancelled = 1;
			req_wait_snd(&snd);
			running = 0;
		}
		xvga_show();
		sys_sleep(10);
	}
	xvga_set_font_bkmode(2, 1);
	req_close();
	return !cancelled;
}
