/* The race's shared state (main.cpp, mycontro.cpp). */
#ifndef A2_RACE_H
#define A2_RACE_H

extern float player_x, player_y, player_z;
extern int player_chunk;
extern int player_heading;


/* objio.cpp: for each of the six racers (player first) its place, 1..6; NULL before the
 * race has started. */
const int *compute_race_ranking(void);

#endif
