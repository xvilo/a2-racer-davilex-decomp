/* The start screen (m_start.cpp): pick, create or delete a player. */
#ifndef A2_M_START_H
#define A2_M_START_H

#include "game.h"

/* first: started with the FIRST command-line flag (first run after installing).
 * Returns 0 when the player chose "Einde". */
int menu_start(game_t *g, int first);

#endif
