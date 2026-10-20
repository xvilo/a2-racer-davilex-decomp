#include "race/bonus.h"

#include <stdio.h>
#include <stdlib.h>

#include "race/object3d.h"
#include "race/opponent.h"
#include "race/race.h"
#include "race/render.h"
#include "race/sndfx.h"
#include "race/statobj.h"
#include "race/weg.h"

/* The original packs these in 25 bytes. */
typedef struct {
	float x, y, z;
	int type;   /* BONUS_*, 0 for a free entry */
	int chunk;  /* -1 */
	int lane;   /* -1 */
	int object; /* -1 */
} bonus_item_t;

static const bonus_item_t empty_bonus = { 0.0f, 0.0f, 0.0f, BONUS_NONE, -1, -1, -1 };

static bonus_item_t *bonus_items;
static int bonus_allocated;
/* What was last collected (unused by the original once written). */
static char bonus_status[8] = "None";

int time_bonus = 300; /* seconds; BONUS/TIME minutes in A2.INI replace it */

/* The static object types of the pickups. */
#define BONUS_OBJECT_MONEY 0x18
#define BONUS_OBJECT_FUEL 0x19
#define BONUS_OBJECT_TIME 0x1a


void bonus_add_time(void)
{
	time_left += time_bonus;
	play_sound_effect(0x1c, 0);
}

void bonus_add_fuel(void)
{
	if (fuel != 0) {
		fuel += 0x4b;
		if (299 < fuel)
			fuel = 300;
	}
	play_sound_effect(0x1d, 0);
}

void bonus_add_money(void)
{
	money += 0xfa;
	if (0x270e < money)
		money = 9999;
	play_sound_effect(0x1e, 0);
}

int alloc_bonus_items(int count)
{
	if (0 < count) {
		bonus_items = malloc((size_t)count * sizeof *bonus_items);
		if (!bonus_items)
			return 2000;
		for (int i = 0; i < count; i++)
			bonus_items[i] = empty_bonus;
		bonus_allocated = count;
	}
	return 0;
}

void free_bonus(void)
{
	if (bonus_allocated == 0)
		return;
	free(bonus_items);
	bonus_items = NULL;
	bonus_allocated = 0;
}

/* Turns a little further every frame; the matrix keeps the turns so far, so the
 * item spins faster and slower as field_3f goes round. */
static int update_bonus_object(int object)
{
	object3d_t *o = &objects3d[object];
	matrix_t m = o->matrix;
	m.m[12] = o->x;
	m.m[13] = o->y;
	m.m[14] = o->z;
	m.m[15] = 1.0f;
	rotate_matrix_y(&m, o->field_3f);
	o->matrix = m;
	o->field_3f = (o->field_3f + 0xf) % 0x400;
	return 0;
}

static int load_bonus_object(int type, float x, float z, int y, int heading)
{
	int object = new_static_object(type);
	object3d_t *o = &objects3d[object];
	o->update = update_bonus_object;
	o->x = x;
	o->y = (float)y;
	o->z = z;
	o->field_3f = heading;
	o->in_use = 1;
	build_object_execute_buffer(object);
	return object;
}

void place_bonus_item(int type, int chunk, int lane)
{
	int object_type = 0;
	if (type == BONUS_NONE) {
		fprintf(stderr, "place_bonus_item: cannot set a bonus object of type none\n");
		exit(1);
	}
	if (chunk < 0 || weg_chunk_count < chunk) {
		fprintf(stderr, "place_bonus_item: chunk value %d out of bounds\n", chunk);
		exit(1);
	}
	if (lane < 0 || 0xb < lane) {
		fprintf(stderr, "place_bonus_item: lane value %d out of bounds\n", lane);
		exit(1);
	}
	switch (type) {
	case BONUS_TIME: object_type = BONUS_OBJECT_TIME; break;
	case BONUS_FUEL: object_type = BONUS_OBJECT_FUEL; break;
	case BONUS_MONEY: object_type = BONUS_OBJECT_MONEY; break;
	default:
		fprintf(stderr, "place_bonus_item: illegal bonus object type\n");
		exit(1);
	}
	int i = 0;
	while (i < bonus_allocated && bonus_items[i].type != BONUS_NONE)
		i++;
	if (i == bonus_allocated) {
		fprintf(stderr, "place_bonus_item: no more free bonus objects\n");
		exit(1);
	}
	bonus_item_t *b = &bonus_items[i];
	const weg_chunk_t *c = &weg_chunks[chunk];
	const vertex3d_t *centre = &vertices3d[weg_sections[chunk].point];
	float l = lane_offsets[lane];
	b->type = type;
	b->chunk = chunk;
	b->lane = lane;
	b->x = c->side[0] * l + centre->x - c->side[2] * 40.0f;
	b->z = c->side[0] * 40.0f + c->side[2] * l + centre->z;
	int heading = 0x100 - weg_sections[chunk].heading;
	int y = (int)road_height_at(chunk, b->x, b->z);
	b->object = load_bonus_object(object_type, b->x, b->z, y, heading);
}

int describe_bonus(int opponent)
{
	if (bonus_allocated == 0) {
		fprintf(stderr, "describe_bonus: no bonus objects allocated\n");
		exit(1);
	}
	const opponent_t *o = &opponent_data[opponent];
	if (o->chunk == -1 || o->lane == -1)
		return BONUS_NONE;
	for (int i = 0; i < bonus_allocated; i++) {
		bonus_item_t *b = &bonus_items[i];
		if (b->type != BONUS_NONE && o->chunk == b->chunk && o->lane == b->lane) {
			int type = b->type;
			objects3d[b->object].update_interval = -1;
			objects3d[b->object].visible = 0;
			objects3d[b->object].in_use = 0;
			*b = empty_bonus;
			return type;
		}
	}
	return BONUS_NONE;
}

void place_all_bonus_items(void)
{
	int count = weg_chunk_count / 0x1b;
	int last_chunk = 0x14;
	if (alloc_bonus_items(count) != 0) {
		fprintf(stderr, "place_all_bonus_items: InitBonus failed\n");
		exit(1);
	}
	int spacing = (weg_chunk_count - 0x28) / count;
	int base = 0x14;
	for (int i = 0; i < count; i++) {
		int chunk = base - rand() % 10;
		int lane = rand() % 4;
		if (lane == 3)
			lane = 2;
		lane++;
		if (track_info[track_index].direction == 0)
			lane = 10 - lane;
		int type = rand() % 4;
		if (type == 0)
			type = BONUS_MONEY;
		/* Fuel at least every 70 chunks: past that, the item becomes fuel. */
		if (type == BONUS_FUEL)
			last_chunk = chunk;
		else if (0x46 < abs(chunk - last_chunk)) {
			type = BONUS_FUEL;
			last_chunk = chunk;
		}
		place_bonus_item(type, chunk, lane);
		base += spacing;
	}
}

void draw_bonus_status(void)
{
	switch (describe_bonus(0)) {
	case BONUS_TIME:
		snprintf(bonus_status, sizeof bonus_status, "Time");
		bonus_add_time();
		break;
	case BONUS_FUEL:
		snprintf(bonus_status, sizeof bonus_status, "Fuel");
		bonus_add_fuel();
		break;
	case BONUS_MONEY:
		snprintf(bonus_status, sizeof bonus_status, "Money");
		bonus_add_money();
		break;
	}
}
