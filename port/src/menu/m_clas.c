/* The overall classification after a race (m_clas.cpp): the player and the five
 * rivals by total time over the stages driven so far, over TStand.pcx (TStand2.pcx
 * after the last stage). */
#include "menu/m_clas.h"

#include <stdlib.h>
#include <string.h>

#include "gfx/screen.h"
#include "gfx/wipes.h"
#include "menu/forms.h"
#include "menu/menu_data.h"
#include "menu/menu_util.h"
#include "menu/mysound.h"
#include "menu/slot.h"
#include "menu/txttool.h"
#include "platform/sys.h"
#include "xvga/font.h"

/* mdc_stand_list's entries (9 bytes each in the original). */
typedef struct {
	int time;
	const char *name;
	char is_player;
} mdc_stand_t;

static mdc_stand_t mdc_stand_list[6];

/* Shortest time first; equal times by name. */
static int mdc_compare_score(const void *a, const void *b)
{
	const mdc_stand_t *p = a, *q = b;
	if (q->time < p->time)
		return 1;
	if (p->time < q->time)
		return -1;
	return strncmp(p->name, q->name, 0x20);
}

static void mdc_create_huidige_stand(void)
{
	const slot_t *s = &slot_list[slot_check_index(active_slot)];
	mdc_stand_list[0].time = slot_count_total_tijd(active_slot, s->stage);
	mdc_stand_list[0].name = s->name;
	mdc_stand_list[0].is_player = 1;
	for (int r = 0; r < 5; r++) {
		mdc_stand_list[r + 1].time =
			slot_count_total_tijd_speler(slot_check_index(active_slot), r);
		mdc_stand_list[r + 1].is_player = 0;
		mdc_stand_list[r + 1].name = speler_namen[r];
	}
	qsort(mdc_stand_list, 6, sizeof mdc_stand_list[0], mdc_compare_score);
}

static void mdc_display_huidige_stand(int dx, int dy, int shadow)
{
	uint32_t normal = shadow ? 0 : 0xafaff, player = shadow ? 0 : 0xa0fff;
	int y = dy + 0x9b;
	xvga_select_font(2);
	for (int i = 0; i < 6; i++, y += 0x19) {
		const mdc_stand_t *e = &mdc_stand_list[i];
		uint32_t c = e->is_player ? player : normal;
		xvga_set_font_colors(2, c, c);
		win_print_xy(1, dx + 0x2c, y, "%d", i + 1);
		win_print_xy(1, dx + 0x52, y, "%s", e->name);
		win_print_xy(1, dx + 0x1e4, y, "%02du%02dm%02ds", e->time / 3600, e->time / 60 % 60,
			e->time % 60);
	}
}

static void mdc_display_score(void)
{
	mdc_create_huidige_stand();
	xvga_select_font(2);
	blt_scr_2_scr(2, 1, 0x2c, 0x9b, 0x270, 0x195, 0);
	mdc_display_huidige_stand(2, 2, 1);
	mdc_display_huidige_stand(1, 1, 1);
	mdc_display_huidige_stand(0, 0, 0);
	blt_scr_2_scr(1, 0, 0x2c, 0x9b, 0x270, 0x195, 0);
	xvga_show();
}

static void mdc_open_screen(game_t *g)
{
	const slot_t *s = &slot_list[slot_check_index(active_slot)];
	load_pcx_file_1(g, 2, s->stage < 6 ? "TStand.pcx" : "TStand2.pcx", 0x1f);
	clear_screen(0, 0);
	handle_wipe(2, WIPE_PIXELFADE, 600);
	mdc_display_score();
	activate_form(mdc_form);
	focus_control(0);
}

static void mdc_close_screen(void)
{
	wait_form_key_released();
	destroy_form();
	clear_screen(2, 0);
	handle_wipe(2, WIPE_PIXELFADE, 600);
}

void menu_display_classe(game_t *g)
{
	int snd = -1;
	mdc_open_screen(g);
	sys_input in = { 0 };
	for (;;) {
		sys_poll(&in);
		int r = handle_form(&in);
		if (r == FORM_ESCAPE || r == FORM_QUIT)
			focus_control(0);
		if (r == 0 || r == FORM_ESCAPE || r == FORM_QUIT) {
			snd_play(&snd, "click1.wav");
			snd_wait(&snd);
			break;
		}
		xvga_show();
		sys_sleep(10);
	}
	mdc_close_screen();
}
