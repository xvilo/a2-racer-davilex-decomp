#include "race/opponent.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "race/camera.h"
#include "race/object3d.h"
#include "race/player.h"
#include "race/race.h"
#include "race/sndfx.h"
#include "race/weg.h"

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

void update_racer_corners(int opponent)
{
	opponent_t *o = &opponent_data[opponent];
	float c = cos_table[o->heading & ANGLE_MASK], s = sin_table[o->heading & ANGLE_MASK];
	for (int i = 0; i < 4; i++) {
		float lx = o->local_corners[i * 3], lz = o->local_corners[i * 3 + 2];
		o->world_corners[i * 3] = (lx * c - lz * s) + o->x;
		o->world_corners[i * 3 + 2] = lz * c + lx * s + o->z;
	}
}

int collide_racer_with_road_edge(int opponent)
{
	opponent_t *o = &opponent_data[opponent];
	const weg_section_t *sec = &weg_sections[o->chunk];
	const vertex3d_t *p = &vertices3d[sec->point];
	float c = cos_table[sec->heading & ANGLE_MASK], s = sin_table[sec->heading & ANGLE_MASK];
	/* How far this frame's move goes across the road. */
	float move = c * o->offset_z + -s * o->offset_x, allowed = move;
	if (move == 0.0f)
		return 0;
	float dist[4];
	for (int i = 0; i < 4; i++)
		dist[i] = fabsf(-s * (o->world_corners[i * 3] - p->x) +
			c * (o->world_corners[i * 3 + 2] - p->z));
	if (o->direction == 1) {
		allowed = -allowed;
		move = -move;
	}
	int hit = 0;
	for (int i = 0; i < 4; i++) {
		if (120.0f < dist[i] + allowed) {
			hit = 1;
			allowed = 120.0f - dist[i];
		}
		if (dist[i] + allowed < 5.0f) {
			hit = 1;
			allowed = 5.0f - dist[i];
		}
	}
	if (!hit)
		return 0;
	o->offset_x *= allowed / move;
	o->offset_z = (allowed / move) * o->offset_z;
	o->speed *= 0.845f;
	if (4.0f < o->speed)
		play_bump3_sound();
	return 1;
}

void collide_player_with_track_ends(void)
{
	opponent_t *o = &opponent_data[0];
	int chunk = o->chunk;
	if (chunk >= 6 && chunk < weg_chunk_count - 6)
		return;
	const weg_section_t *sec = &weg_sections[chunk];
	const vertex3d_t *p = &vertices3d[sec->point];
	float c = cos_table[sec->heading & ANGLE_MASK], s = sin_table[sec->heading & ANGLE_MASK];
	float move = s * o->offset_z + c * o->offset_x, allowed = move;
	if (move == 0.0f)
		return;
	int hit = 0;
	for (int i = 0; i < 4; i++) {
		float d = fabsf(c * (o->world_corners[i * 3] - p->x) +
			s * (o->world_corners[i * 3 + 2] - p->z));
		if (o->direction == 1) {
			if (d + allowed < 1.0f) {
				hit = 1;
				allowed = 1.0f - d;
			}
		}
		else if (59.0f < d + allowed) {
			hit = 1;
			allowed = 59.0f - d;
		}
	}
	if (!hit)
		return;
	o->offset_x *= allowed / move;
	o->offset_z = (allowed / move) * o->offset_z;
	o->speed *= 0.845f;
	if (4.0f < o->speed)
		play_bump3_sound();
}

int get_racer_lane(int opponent)
{
	static const float bounds[12] = {
		-120.0f, -94.5f, -72.5f, -45.0f, -15.0f, 0.0f, 0.0f, 15.0f, 45.0f, 72.5f, 94.5f, 120.0f,
	};
	const opponent_t *o = &opponent_data[opponent];
	const weg_section_t *sec = &weg_sections[o->chunk];
	const vertex3d_t *p = &vertices3d[sec->point];
	int h = sec->heading & ANGLE_MASK;
	float d = (o->z - p->z) * cos_table[h] + -sin_table[h] * (o->x - p->x);
	for (int i = 0; i < 11; i++) {
		if (bounds[i] <= d && d < bounds[i + 1])
			return i;
	}
	return -1;
}

/* update_racers. TODO: the rivals, police and traffic (their drivers, the active list
 * and collisions between cars); for now only the player moves. */
void update_racers(void)
{
	int heading = player_heading;
	int op = objects3d[player_index].opponent;
	opponent_t *o = &opponent_data[op];
	handle_player_controls();
	o->x = player_x;
	o->y = player_y;
	o->z = player_z;
	o->heading = heading;
	o->heading_delta = player_heading - heading;
	o->speed = player_speed;
	o->chunk = player_chunk;
	o->lane = get_racer_lane(op);

	/* Each active car turns and gets its move for this frame. */
	o->heading += o->heading_delta;
	update_racer_corners(op);
	o->heading_delta = 0;
	o->field_18 = cos_table[(o->heading + 0x100) & ANGLE_MASK];
	o->field_1c = 0;
	o->field_20 = sin_table[(o->heading + 0x100) & ANGLE_MASK];
	o->offset_x = o->field_18 * o->speed + o->push_x;
	o->offset_z = o->field_20 * o->speed + o->push_z;
	collide_racer_with_road_edge(op);
	if (op == 0)
		collide_player_with_track_ends();

	/* Then everyone moves. */
	o->x += o->offset_x;
	o->z += o->offset_z;
	update_racer_corners(op);

	o->push_x *= 0.75f;
	o->push_z *= 0.75f;
	player_x = o->x;
	player_z = o->z;
	player_y = o->y;
	player_speed = o->speed;
	player_heading = o->heading;
	select_rpm_step_for_speed(player_speed);
	update_camera();
}
