#include "menu/score.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "menu/menu_data.h"
#include "xvga/fileio.h"

/* score_t, as SCORE.A2R holds it. */
static struct {
	single_score_t hiscore[SCORE_COUNT];
	single_score_t levelscore[6][SCORE_COUNT];
} score_db;

typedef char score_size_check[sizeof score_db == 0xc08 ? 1 : -1];

/* The lists start out with the developers' names (score_list in A2MENU.DAT). */
static void default_scores(void)
{
	memcpy(&score_db, score_list, sizeof score_db);
}

void save_tmp_scoredbase(const char *name)
{
	if (file_write(&score_db, sizeof score_db, name))
		fprintf(stderr, "save_scoredbase: cannot write %s\n", save_path(name));
}

void save_scoredbase(void)
{
	save_tmp_scoredbase("SCORE.A2R");
}

void load_tmp_scoredbase(const char *name)
{
	if (file_read(&score_db, sizeof score_db, name)) {
		file_delete(name);
		init_scoredbase();
	}
}

void init_scoredbase(void)
{
	if (file_exist("SCORE.A2R"))
		load_tmp_scoredbase("SCORE.A2R");
	else {
		default_scores();
		save_scoredbase();
	}
	/* Nothing is new when the menu starts. */
	for (int i = 0; i < SCORE_COUNT; i++) {
		for (int s = 0; s < 6; s++)
			score_db.levelscore[s][i].is_new = 0;
		score_db.hiscore[i].is_new = 0;
	}
}

static int get_last_entered(const single_score_t *list)
{
	for (int i = 0; i < SCORE_COUNT; i++) {
		if (list[i].is_new)
			return i;
	}
	return -1;
}

int get_last_entered_hiscore(void)
{
	return get_last_entered(score_db.hiscore);
}

int get_last_entered_levelscore(int stage)
{
	return get_last_entered(score_db.levelscore[stage]);
}

/* Further stage first, then the shorter time, then by name. */
static int compare_score(const void *pa, const void *pb)
{
	const single_score_t *a = pa, *b = pb;
	if (a->stage != b->stage)
		return a->stage < b->stage ? 1 : -1;
	if (a->time != b->time)
		return a->time > b->time ? 1 : -1;
	return strncmp(a->name, b->name, sizeof a->name);
}

/* Adds an entry, sorts the eleven and keeps the best ten. */
static void add_score(single_score_t *list, const char *name, int time, int stage, int money)
{
	single_score_t buf[SCORE_COUNT + 1];
	for (int i = 0; i < SCORE_COUNT; i++)
		list[i].is_new = 0;
	memcpy(buf, list, sizeof(single_score_t) * SCORE_COUNT);
	memset(&buf[SCORE_COUNT], 0, sizeof buf[SCORE_COUNT]);
	snprintf(buf[SCORE_COUNT].name, sizeof buf[SCORE_COUNT].name, "%s", name);
	buf[SCORE_COUNT].time = time;
	buf[SCORE_COUNT].stage = (short)stage;
	buf[SCORE_COUNT].is_new = 1;
	buf[SCORE_COUNT].money = money;
	qsort(buf, SCORE_COUNT + 1, sizeof buf[0], compare_score);
	memcpy(list, buf, sizeof(single_score_t) * SCORE_COUNT);
}

int add_new_hiscore(const char *name, int time, int stage, int money)
{
	add_score(score_db.hiscore, name, time, stage, money);
	return get_last_entered_hiscore();
}

int add_new_levelscore(const char *name, int time, int stage, int money)
{
	/* A stage list keeps 0 as the stage of every entry. */
	add_score(score_db.levelscore[stage], name, time, 0, money);
	return get_last_entered_levelscore(stage);
}

single_score_t *score_hiscore(void)
{
	return score_db.hiscore;
}

single_score_t *score_levelscore(int stage)
{
	return score_db.levelscore[stage];
}
