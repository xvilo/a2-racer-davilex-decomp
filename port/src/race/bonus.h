/* Bonus items (bonus.cpp): spinning time, fuel and money pickups on the road. Driving
 * over one (same chunk and lane) collects it. */
#ifndef A2_BONUS_H
#define A2_BONUS_H

enum {
	BONUS_NONE = 0,
	BONUS_TIME = 1,
	BONUS_FUEL = 2,
	BONUS_MONEY = 3,
};

extern int time_bonus;

void bonus_add_time(void);
void bonus_add_fuel(void);
void bonus_add_money(void);

int alloc_bonus_items(int count);
void free_bonus(void);
/* A bonus of the type at the chunk, in the lane (0..10). */
void place_bonus_item(int type, int chunk, int lane);
/* The bonus the racer is on, now taken off the road, or BONUS_NONE. */
int describe_bonus(int opponent);
/* One per 27 chunks, spread along the road. */
void place_all_bonus_items(void);
/* Collects the bonus the player is on. */
void draw_bonus_status(void);

#endif
