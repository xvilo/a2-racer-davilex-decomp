/* Between the menu and the race (spel.cpp): reset_infio fills INFIO.INI's data for
 * the race; handle_game takes the result back into the player's slot. */
#include "menu/spel.h"

#include "gfx/screen.h"
#include "gfx/wipes.h"
#include "menu/infio.h"
#include "menu/m_clas.h"
#include "menu/m_score.h"
#include "menu/m_stand.h"
#include "menu/mysound.h"
#include "menu/optie.h"
#include "menu/score.h"
#include "menu/slot.h"

void reset_infio(void)
{
	const slot_t *s = &slot_list[slot_check_index(active_slot)];
	infio.race_finished = 0;
	infio.money = s->money;
	infio.active_slot = (int16_t)active_slot;
	infio.sound = opties.sound;
	infio.resolution = opties.resolution;
	infio.view = opties.view;
	infio.control_type = (int16_t)control.control_type;
	infio.display_mode = opties.display_mode;
	infio.horizon = opties.horizon;
	infio.dashboard = opties.dashboard;
	infio.detail = opties.detail;
	infio.camera = opties.camera;
	infio.music_volume = opties.music_volume;
	infio.effects_volume = opties.effects_volume;
	infio.stage = s->stage;
	/* Starting order: the result of the previous stage, or 1..6 on the first. */
	if (s->stage < 1) {
		infio.position = 6;
		infio.racer_1 = 1;
		infio.racer_2 = 2;
		infio.racer_3 = 3;
		infio.racer_4 = 4;
		infio.racer_5 = 5;
	}
	else {
		int prev = slot_check_level(s->stage - 1);
		infio.position = s->position[prev];
		infio.racer_1 = s->racer[0][prev];
		infio.racer_2 = s->racer[1][prev];
		infio.racer_3 = s->racer[2][prev];
		infio.racer_4 = s->racer[3][prev];
		infio.racer_5 = s->racer[4][prev];
	}
	infio.time = 0;
	infio.time_racer_1 = 0;
	infio.time_racer_2 = 0;
	infio.time_racer_3 = 0;
	infio.time_racer_4 = 0;
	infio.time_racer_5 = 0;
	infio.final = 0;
	infio.ended_by = 0;
	infio.key_left = (int16_t)control.key_left;
	infio.key_right = (int16_t)control.key_right;
	infio.key_gas = (int16_t)control.key_gas;
	infio.key_brake = (int16_t)control.key_brake;
	infio.key_action1 = (int16_t)control.key_action1;
	infio.key_action2 = (int16_t)control.key_action2;
}

/* movie_play_outtro: empty in the original; the launcher plays a2win.smk itself. */
static void movie_play_outtro(void)
{
}

int handle_game(game_t *g)
{
	int snd = -1;
	active_slot = infio.active_slot;
	if (!infio.race_finished)
		return 1;
	infio.race_finished = 0;
	slot_t *s = &slot_list[slot_check_index(active_slot)];
	int stage = s->stage;
	s->stage++;
	s->money = infio.money;
	int level = slot_check_level(stage);
	s->time[level] = infio.time;
	s->position[level] = infio.position;
	s->racer[0][level] = infio.racer_1;
	s->racer[1][level] = infio.racer_2;
	s->racer[2][level] = infio.racer_3;
	s->racer[3][level] = infio.racer_4;
	s->racer[4][level] = infio.racer_5;
	s->time_racer[0][level] = infio.time_racer_1;
	s->time_racer[1][level] = infio.time_racer_2;
	s->time_racer[2][level] = infio.time_racer_3;
	s->time_racer[3][level] = infio.time_racer_4;
	s->time_racer[4][level] = infio.time_racer_5;

	snd_play_looping(&snd, "rock.wav");
	menu_display_stand(g, stage);
	menu_display_classe(g);
	add_new_levelscore(s->name, s->time[level], stage, s->money);
	int at = get_last_entered_levelscore(stage);
	if (at >= 0 && at < 10)
		menu_display_score(g, stage);
	while (snd_fade_out(&snd))
		;

	if (s->stage > 5) {
		/* The last stage: the series goes into the high scores and the slot is freed. */
		movie_play_outtro();
		clear_screen(2, 0);
		handle_wipe(2, WIPE_PIXELFADE, 600);
		clear_screen(1, 0);
		add_new_hiscore(s->name, slot_count_total_tijd(active_slot, s->stage), s->stage,
			s->money);
		at = get_last_entered_hiscore();
		if (at >= 0 && at < 10)
			menu_display_score(g, -1);
		clear_slot(active_slot);
		active_slot = get_last_saved_slot();
		return 0;
	}
	return 1;
}
