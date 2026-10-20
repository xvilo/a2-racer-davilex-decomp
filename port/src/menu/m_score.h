/* The score screens (m_score.cpp, m_dscore.cpp). */
#ifndef A2_M_SCORE_H
#define A2_M_SCORE_H

#include "game.h"

/* The score menu, opened on a stage's list (0..5) or the overall list (-1). */
int menu_score(game_t *g, int stage);
/* The list shown after a race, with only a back button. */
void menu_display_score(game_t *g, int stage);

#endif
