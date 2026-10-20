/* Between the menu and the race (spel.cpp). */
#ifndef A2_SPEL_H
#define A2_SPEL_H

#include "game.h"

/* Fills infio from the active player's slot and the options before a race. */
void reset_infio(void);
/* After a race: stores its result in the player's slot and shows the stage result,
 * the classification and the scores. Returns 0 when the series is over (the player
 * then picks a slot again), 1 to go on to the main menu. */
int handle_game(game_t *g);

#endif
