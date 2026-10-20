#include "race/pion.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "race/driver.h"
#include "race/object3d.h"
#include "race/opponent.h"
#include "race/race.h"
#include "race/render.h"
#include "race/sndfx.h"
#include "race/statobj.h"
#include "race/weg.h"

/* 0x2c bytes in the original. */
typedef struct {
	float x, y, z;
	int field_0c, field_10, field_14;
	int in_use;
	int hit;    /* knocked over, flying */
	int chunk;
	int lane;
	int object;
} pion_t;

static const pion_t empty_pion = { 0.0f, 0.0f, 0.0f, 0, 0, 0, 0, -1, -1, -1, 0 };

static pion_t *pionnen;
static int pion_count;

/* The static object types: the cone and the road works sign. */
#define PION_OBJECT_LEFT 0x13  /* werkweg.d3d */
#define PION_OBJECT_RIGHT 0x14 /* werkweg2.d3d */

int alloc_pionnen(int count)
{
	if (0 < count) {
		pionnen = malloc((size_t)count * sizeof *pionnen);
		if (!pionnen)
			return 2000;
		for (int i = 0; i < count; i++)
			pionnen[i] = empty_pion;
		pion_count = count;
	}
	return 0;
}

void free_pionnen(void)
{
	free(pionnen);
	pionnen = NULL;
	pion_count = 0;
}

void init_pionnen(void)
{
	if (alloc_pionnen(0x3c) != 0) {
		fprintf(stderr, "init_pionnen: Initpion failed\n");
		exit(1);
	}
}

/* Moves by its velocity and tumbles by its three angles; like a bonus item's, the
 * matrix keeps the turns so far. */
static int update_pion_object(int object)
{
	object3d_t *o = &objects3d[object];
	o->x += o->vx;
	o->y += o->vy;
	o->z += o->vz;
	matrix_t m = o->matrix;
	m.m[12] = o->x;
	m.m[13] = o->y;
	m.m[14] = o->z;
	m.m[15] = 1.0f;
	if (o->field_3f != 0)
		rotate_matrix_y(&m, o->field_3f);
	if (o->field_3b != 0)
		rotate_matrix_x(&m, o->field_3b);
	if (o->field_43 != 0)
		rotate_matrix_z(&m, o->field_43);
	o->matrix = m;
	return 0;
}

static int load_pion_object(int type, float x, float z, int y)
{
	int object = new_static_object(type);
	object3d_t *o = &objects3d[object];
	o->update = update_pion_object;
	o->x = x;
	o->y = (float)y + 0.1f;
	o->z = z;
	o->field_3f = 0;
	o->field_3b = 0;
	o->field_43 = 0;
	o->in_use = 1;
	build_object_execute_buffer(object);
	return object;
}

/* place_pion_left and place_pion_right differ only in the object type. */
static void place_pion(const char *who, int type, int chunk, int lane)
{
	if (chunk < 0 || weg_chunk_count < chunk) {
		fprintf(stderr, "%s: chunk value out of bounds\n", who);
		exit(1);
	}
	if (lane < 0 || 0xb < lane) {
		fprintf(stderr, "%s: lane value out of bounds\n", who);
		exit(1);
	}
	int i = 0;
	while (i < pion_count && pionnen[i].in_use != 0)
		i++;
	if (i == pion_count) {
		fprintf(stderr, "%s: no more free pion objects\n", who);
		exit(1);
	}
	pion_t *p = &pionnen[i];
	const weg_chunk_t *c = &weg_chunks[chunk];
	const vertex3d_t *centre = &vertices3d[weg_sections[chunk].point];
	float l = lane_offsets[lane];
	p->chunk = chunk;
	p->lane = lane;
	p->x = c->side[0] * l + centre->x - c->side[2] * 40.0f;
	p->in_use = 1;
	p->hit = 0;
	p->z = c->side[0] * 40.0f + c->side[2] * l + centre->z;
	int y = (int)road_height_at(chunk, p->x, p->z);
	p->object = load_pion_object(type, p->x, p->z, y);
}

void place_pion_left(int chunk, int lane)
{
	place_pion("place_pion_left", PION_OBJECT_LEFT, chunk, lane);
}

void place_pion_right(int chunk, int lane)
{
	place_pion("place_pion_right", PION_OBJECT_RIGHT, chunk, lane);
}

void collide_with_pionnen(int opponent)
{
	if (pion_count == 0) {
		fprintf(stderr, "collide_with_pionnen: no pion objects allocated\n");
		exit(1);
	}
	const opponent_t *op = &opponent_data[opponent];
	if (op->chunk == -1 || op->lane == -1)
		return;
	for (int i = 0; i < pion_count; i++) {
		pion_t *p = &pionnen[i];
		if (p->in_use == 0)
			continue;
		object3d_t *o = &objects3d[p->object];
		if (p->hit != 0) {
			o->vy += -0.125f;
			o->vx *= 0.95f;
			o->vz *= 0.95f;
			o->field_3f = (o->field_3f + (rand() & 0xf)) % 0x400;
			o->field_3b = (o->field_3b + (rand() & 0xf)) % 0x400;
			o->field_43 = (o->field_43 + (rand() & 0xf)) % 0x400;
		}
		if (o->y <= 0.0f) {
			o->update_interval = -1;
			o->visible = 0;
			o->in_use = 0;
			*p = empty_pion;
			p->in_use = 0;
			p->hit = 0;
		}
		if (p->chunk == op->chunk && p->lane == op->lane) {
			o->vx = op->field_18 * op->speed;
			o->vz = op->field_20 * op->speed;
			o->vy = (fabsf(o->vz) + fabsf(o->vx)) / 3.0f;
			if (p->hit == 0)
				play_bump_sound();
			p->hit = 1;
			break;
		}
	}
}

void place_pion_group(int chunk)
{
	int direction = track_info[track_index].direction;
	int c, lane;
	if (direction == 1) {
		c = chunk - 1;
		lane = 0;
		if (c < 0)
			c = 0;
	}
	else {
		c = chunk + 1;
		lane = 10;
		if (weg_section_count <= c)
			c = weg_section_count - 1;
	}
	place_pion_right(c, lane);
	for (int i = 0; i < 10; i++) {
		if (direction == 1) {
			c = chunk + i;
			lane = rand() % 2 != 0 ? 1 : 0;
		}
		else {
			c = chunk - i;
			lane = rand() % 2 == 0 ? 10 : 9;
		}
		if (c < 0)
			c = 0;
		if (weg_section_count <= c)
			c = weg_section_count - 1;
		place_pion_left(c, lane);
	}
}

void place_random_pionnen(void)
{
	int groups = rand() % 2 + 4;
	int step = weg_chunk_count / 0x14;
	int along = 0;
	for (int i = 0; i < groups; i++) {
		double chunk = (double)step + (double)along * 1.5;
		along += step;
		place_pion_group((int)chunk);
	}
}

void update_pionnen(void)
{
	collide_with_pionnen(0);
	for (int i = 1; i < opponents_used; i++) {
		int kind = opponent_data[i].field_e4;
		if (kind == DRIVER_RIVAL || kind == DRIVER_POLICE)
			collide_with_pionnen(i);
	}
}
