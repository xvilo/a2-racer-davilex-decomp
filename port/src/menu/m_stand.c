/* The stage result after a race (m_stand.cpp): positions 1 to 6 of the stage just
 * driven, with name and time, over that stage's picture (Etappe1..6.pcx). */
#include "menu/m_stand.h"

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

/* One pass of the table at an offset; passes at (2,2) and (1,1) in black, then (0,0)
 * in colour, give the shadow. The player's row is drawn in another colour. */
static void mdt_display_stand(int stage, int dx, int dy, int shadow)
{
	static const char leeg[4] = "";
	uint32_t normal = shadow ? 0 : 0xafaff, player = shadow ? 0 : 0xa0fff;
	int y = dy + 0x9b;
	if (stage < 0)
		return;
	xvga_select_font(2);
	const slot_t *s = &slot_list[slot_check_index(active_slot)];
	int level = slot_check_level(stage);
	for (int pos = 1; pos < 7; pos++, y += 0x19) {
		uint32_t c = normal;
		const char *name = leeg;
		int time = 0;
		if (s->position[level] == pos) {
			c = player;
			name = s->name;
			time = s->time[level];
		}
		for (int r = 0; r < 5; r++) {
			if (s->racer[r][level] == pos) {
				name = speler_namen[r];
				time = s->time_racer[r][level];
			}
		}
		xvga_set_font_colors(2, c, c);
		win_print_xy(1, dx + 0x2c, y, "%d", pos);
		win_print_xy(1, dx + 0x52, y, "%s", name);
		win_print_xy(1, dx + 0x1e4, y, "%02du%02dm%02ds", time / 3600, time / 60 % 60,
			time % 60);
	}
}

static void mdt_display_score(int stage)
{
	xvga_select_font(2);
	blt_scr_2_scr(2, 1, 0x2c, 0x9b, 0x270, 0x195, 0);
	mdt_display_stand(stage, 2, 2, 1);
	mdt_display_stand(stage, 1, 1, 1);
	mdt_display_stand(stage, 0, 0, 0);
	blt_scr_2_scr(1, 0, 0x2c, 0x9b, 0x270, 0x195, 0);
	xvga_show();
}

static void mdt_open_screen(game_t *g, int stage)
{
	load_pcx_file_1(g, 2, mdt_stand_pcx[stage % 6], 0x1f);
	clear_screen(0, 0);
	handle_wipe(2, WIPE_PIXELFADE, 600);
	mdt_display_score(stage);
	activate_form(mdt_form);
	focus_control(0);
}

static void mdt_close_screen(void)
{
	wait_form_key_released();
	destroy_form();
	clear_screen(2, 0);
	handle_wipe(2, WIPE_PIXELFADE, 600);
}

void menu_display_stand(game_t *g, int stage)
{
	int snd = -1;
	mdt_open_screen(g, stage);
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
	mdt_close_screen();
}
