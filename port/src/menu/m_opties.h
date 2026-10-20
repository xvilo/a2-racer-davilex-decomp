/* The options menu (m_opties.cpp). */
#ifndef A2_M_OPTIES_H
#define A2_M_OPTIES_H

#include "game.h"

/* music: the menu music's sound handle, restarted or stopped as the sound option
 * changes; tune: its file. */
int menu_opties(game_t *g, int *music, const char *tune);

#endif
