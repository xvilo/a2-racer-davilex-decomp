#include "race/opponent.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "race/object3d.h"

opponent_t *opponent_data;
int opponent_count;
static int opponents_used;

int init_opponents(int count)
{
	if (opponent_data)
		return 0;
	opponent_data = calloc((size_t)count, sizeof *opponent_data);
	if (!opponent_data)
		return 2000;
	opponent_count = count;
	opponents_used = 0;
	for (int i = 0; i < count; i++) {
		opponent_data[i].field_e0 = -1;
		opponent_data[i].active = 0;
	}
	return 0;
}

void exit_opponents(void)
{
	free(opponent_data);
	opponent_data = NULL;
	opponent_count = opponents_used = 0;
}

int alloc_opponent_slot(void)
{
	if (opponents_used < opponent_count)
		return opponents_used++;
	for (int i = 0; i < opponent_count; i++) {
		if (opponent_data[i].field_e0 == -1 && !opponent_data[i].active)
			return i;
	}
	fprintf(stderr, "no more free opponent slots\n");
	exit(1);
}

void compute_object_bounds(int object)
{
	const object3d_t *o = &objects3d[object];
	const frame3d_t *fr = object_frame(object);
	float min_x = 10000.0f, min_z = 10000.0f, max_x = 0.0f, max_z = 0.0f;
	if (o->opponent < 0 || o->opponent > opponent_count) {
		fprintf(stderr, "illegal opponent %d\n", o->opponent);
		exit(1);
	}
	/* The original tests the in-use flag of pool vertex i rather than of the frame's
	 * i-th vertex; those are always in use, so every vertex counts. */
	for (int i = 0; i < fr->vertex_count; i++) {
		const vertex3d_t *v = &vertices3d[fr->vertices[i]];
		if (v->x < min_x)
			min_x = v->x;
		if (v->z < min_z)
			min_z = v->z;
		if (max_x < v->x)
			max_x = v->x;
		if (max_z < v->z)
			max_z = v->z;
	}
	opponent_t *op = &opponent_data[o->opponent];
	float corners[12] = {
		min_x, 0, min_z, max_x, 0, min_z, max_x, 0, max_z, min_x, 0, max_z,
	};
	for (int i = 0; i < 12; i++)
		op->local_corners[i] = corners[i];
	op->radius = sqrtf(max_z * max_z + max_x * max_x);
}
