/* The race's shared state. The race loop itself (main.cpp's WinMain) comes here as the
 * port grows. */
#include "race/race.h"

#include <stddef.h>

float player_x, player_y, player_z;
int player_chunk;
int player_heading;

const int *compute_race_ranking(void)
{
	return NULL; /* not ported yet: no racers on the road */
}
