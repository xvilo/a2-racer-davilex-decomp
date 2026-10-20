/* The score screens: menu_score with its tabs (m_score.cpp) and menu_display_score,
 * shown after a race (m_dscore.cpp). */
#include "menu/m_score.h"

#include <stdio.h>

#include "gfx/screen.h"
#include "gfx/wipes.h"
#include "menu/forms.h"
#include "menu/menu_data.h"
#include "menu/menu_util.h"
#include "menu/mysound.h"
#include "menu/score.h"
#include "menu/txttool.h"
#include "platform/sys.h"
#include "xvga/font.h"

/* Controls of ms_form: 0 back ("Terug"), 1 the overall list, 2..7 stages 1..6. */
enum { MS_BACK = 0, MS_OVERALL = 1, MS_STAGE1 = 2 };

static int ms_get_current_display(int stage)
{
	return stage >= 0 && stage <= 5 ? stage : -1;
}

/* How the four print functions of the original lay out a table. */
typedef struct {
	int top, row;          /* first row and row height */
	uint32_t new_color;    /* the entry added last; others are 0xafaff */
	const char *rank_fmt;
	int name_width;
	int level_x;           /* stage name column, -1 for none */
	int money_x;           /* -1 for none */
	int money_width;       /* > 0: shortened to fit (win_bprintf) */
} score_layout;

static const score_layout ms_score = { 0xb1, 0x17, 0xa0ffa, "%2d", 0xb0, -1, 0x10c, 0 };
static const score_layout ms_hiscore = { 0xb1, 0x17, 0xa0ffa, "%d", 0x84, 0xe0, 0x162, 0 };
static const score_layout mds_score = { 0x9b, 0x19, 0xa0fff, "%2d", 0x84, -1, 0x10c, 0x96 };
static const score_layout mds_hiscore = { 0x9b, 0x19, 0xa0fff, "%2d", 0x84, 0xe0, -1, 0 };

/* Ten rows at an offset; passes at (2,2) and (1,1) in black, then (0,0) in colour,
 * give the shadow. */
static void print_scores(const single_score_t *s, const score_layout *l, int d, int shadow)
{
	uint32_t normal = shadow ? 0 : 0xafaff, fresh = shadow ? 0 : l->new_color;
	int y = d + l->top;
	for (int rank = 1; rank <= SCORE_COUNT; rank++, s++, y += l->row) {
		uint32_t c = s->is_new ? fresh : normal;
		xvga_set_font_colors(2, c, c);
		win_print_xy(1, d + 0x2c, y, l->rank_fmt, rank);
		win_bprintf(1, d + 0x52, y, l->name_width, "%s", s->name);
		if (l->level_x >= 0)
			win_print_xy(1, d + l->level_x, y, "%s", level_naam[s->stage]);
		if (l->money_x >= 0 && l->money_width > 0)
			win_bprintf(1, d + l->money_x, y, l->money_width, "%9d", s->money);
		else if (l->money_x >= 0)
			win_print_xy(1, d + l->money_x, y, "%9d", s->money);
		win_print_xy(1, d + 0x1e4, y, "%02du%02dm%02ds", s->time / 3600, s->time / 60 % 60,
			s->time % 60);
	}
}

/* --- menu_score (m_score.cpp) ----------------------------------------------------- */

static void ms_display_score(int display)
{
	xvga_select_font(2);
	blt_scr_2_scr(2, 1, 0x2c, 0xb1, 0x25c, 0x197, 0);
	const single_score_t *list = display < 0 ? score_hiscore() : score_levelscore(display);
	for (int pass = 2; pass >= 0; pass--)
		print_scores(list, display < 0 ? &ms_hiscore : &ms_score, pass, pass > 0);
	/* The tab row (anim 0x50: stages 0..5, 6 overall) and the column heads (anim 0x51). */
	set_mem_image(0, anims[0x50].frames[display < 0 ? 6 : display].image, 0, 0x70, MI_TOPLEFT);
	set_mem_image(0, anims[0x51].frames[display < 0 ? 0 : 1].image, 0, 0x99, MI_TOPLEFT);
	blt_scr_2_scr(1, 0, 0x2c, 0xb1, 0x25c, 0x197, 0);
	xvga_show();
}

int menu_score(game_t *g, int stage)
{
	int snd = -1, display = ms_get_current_display(stage);
	load_pcx_file_1(g, 2, "Score.pcx", 0x1f);
	handle_wipe(2, WIPE_PIXELFADE, 600);
	ms_display_score(display);
	activate_form(ms_form);
	focus_control(MS_BACK);
	sys_input in = { 0 };
	for (;;) {
		sys_poll(&in);
		int r = handle_form(&in);
		if (r == FORM_ESCAPE || r == FORM_QUIT) {
			focus_control(MS_BACK);
			r = MS_BACK;
		}
		if (r == MS_BACK) {
			snd_play(&snd, "click1.wav");
			snd_wait(&snd);
			break;
		}
		if (r == MS_OVERALL && display != -1) {
			display = -1;
			snd_play(&snd, "click3.wav");
			wait_form_key_released();
			ms_display_score(display);
			snd_wait(&snd);
		}
		else if (r >= MS_STAGE1 && r < MS_STAGE1 + 6 && display != r - MS_STAGE1) {
			display = r - MS_STAGE1;
			snd_play(&snd, "click3.wav");
			wait_form_key_released();
			ms_display_score(display);
			snd_wait(&snd);
		}
		xvga_show();
		sys_sleep(10);
	}
	wait_form_key_released();
	destroy_form();
	return 0;
}

/* --- menu_display_score (m_dscore.cpp) --------------------------------------------- */

static void mds_display_score(int display)
{
	xvga_select_font(2);
	blt_scr_2_scr(2, 1, 0x2c, 0x9b, 0x270, 0x195, 0);
	const single_score_t *list = display < 0 ? score_hiscore() : score_levelscore(display);
	for (int pass = 2; pass >= 0; pass--)
		print_scores(list, display < 0 ? &mds_hiscore : &mds_score, pass, pass > 0);
	blt_scr_2_scr(1, 0, 0x2c, 0x9b, 0x270, 0x195, 0);
	xvga_show();
}

void menu_display_score(game_t *g, int stage)
{
	int snd = -1, display = ms_get_current_display(stage);
	load_pcx_file_1(g, 2, display == -1 ? "Score3.pcx" : "Score2.pcx", 0x1f);
	clear_screen(0, 0);
	handle_wipe(2, WIPE_PIXELFADE, 600);
	mds_display_score(display);
	activate_form(mds_form);
	focus_control(0);
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
	wait_form_key_released();
	destroy_form();
	clear_screen(2, 0);
	handle_wipe(2, WIPE_PIXELFADE, 600);
}
