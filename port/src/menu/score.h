/* High scores (score.cpp), kept in SCORE.A2R: the overall top ten and a top ten per
 * stage. */
#ifndef A2_SCORE_H
#define A2_SCORE_H

#include "menu/menu_types.h"

#define SCORE_COUNT 10

single_score_t *score_hiscore(void);
single_score_t *score_levelscore(int stage);

void init_scoredbase(void);
void save_scoredbase(void);
void save_tmp_scoredbase(const char *name);
void load_tmp_scoredbase(const char *name);
/* Rank of the entry added last, -1 when it did not make the list. */
int get_last_entered_hiscore(void);
int get_last_entered_levelscore(int stage);
int add_new_hiscore(const char *name, int time, int stage, int money);
int add_new_levelscore(const char *name, int time, int stage, int money);

#endif
