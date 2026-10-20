#include "race/opponent.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "race/camera.h"
#include "race/driver.h"
#include "race/object3d.h"
#include "race/player.h"
#include "race/race.h"
#include "race/render.h"
#include "race/sndfx.h"
#include "race/weg.h"

opponent_t *opponent_data;
int opponent_count;
static int opponents_used;
int police_max_speed = 17, racer_speed = 16;

/* The cars near the player that move this frame, and those near enough to collide. */
static int *active_opponent_list, *nearby_opponent_list;
static int active_opponent_count, nearby_opponent_count, active_list_size;

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
		if (!opponent_data[i].active)
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


/* --- creating and placing (opponent.cpp) ----------------------------------------- */

static const float lane_offsets[11] = {
	108.0f, 85.0f, 60.0f, 30.0f, 0.0f, 0.0f, 0.0f, -30.0f, -60.0f, -85.0f, -108.0f,
};

/* The road's slope under a car: its pitch and roll (FUN_00405058); returns the road's
 * height there. */
static float opponent_slope(int opponent, int heading, int *pitch, int *roll)
{
	const opponent_t *o = &opponent_data[opponent];
	int chunk = update_chunk_for_position(o->chunk, o->x, o->z);
	float y = road_height_at(chunk, o->x, o->z);
	int h = heading & ANGLE_MASK;
	float fx = o->x + cos_table[h] * 8.0f, fz = sin_table[h] * 8.0f + o->z;
	float v[3] = { fx - o->x, road_height_at(chunk, fx, fz) - y, fz - o->z };
	normalize_vector(v);
	*pitch = (int)(v[1] * 128.0f);
	int s = (heading + 0x100) & ANGLE_MASK;
	float sx = cos_table[s] * 4.0f, sz = sin_table[s] * 4.0f;
	float a = road_height_at(chunk, o->x + sx, o->z + sz);
	float b = road_height_at(chunk, o->x - sx, o->z - sz);
	float w[3] = { 2 * sx, a - b, 2 * sz };
	normalize_vector(w);
	*roll = (int)(w[1] * 128.0f);
	return y;
}

static int update_opponent_object(int object)
{
	object3d_t *ob = &objects3d[object];
	int op = ob->opponent;
	opponent_t *o = &opponent_data[op];
	int heading = 0x3ff - ((o->heading + o->steer_angle) & 0x3ff), pitch = 0, roll = 0;
	if (ob->visible)
		o->y = opponent_slope(op, heading, &pitch, &roll);
	ob->x = o->x;
	ob->y = o->y;
	ob->z = o->z;
	if (ob->visible) {
		matrix_t m;
		matrix_identity(&m);
		m.m[12] = ob->x;
		m.m[13] = ob->y;
		m.m[14] = ob->z;
		rotate_matrix_y(&m, heading);
		rotate_matrix_x(&m, -pitch);
		rotate_matrix_z(&m, roll);
		ob->matrix = m;
	}
	return 0;
}

/* create_opponent: a car of a type, for a kind of driver; returns its object. */
int create_opponent(int car_type, int kind)
{
	int object = get_new_object3d();
	object3d_t *ob = &objects3d[object];
	ob->in_use = 1;
	int op = alloc_opponent_slot();
	opponent_t *o = &opponent_data[op];
	car_type_t *ct = &car_types[car_type];
	o->car_type = car_type;
	ob->anim = init_anim3d(1);
	ob->frame = 0;
	if (ct->mesh_id == -1) {
		load_object_mesh(object, ct->mesh);
		ct->mesh_id = anims3d[ob->anim].frames[ob->frame];
	}
	else {
		anims3d[ob->anim].frames[ob->frame] = ct->mesh_id;
	}
	o->object = object;
	o->car_class = ct->car_class;
	o->max_speed = car_type == 0xc ? (float)police_max_speed : ct->max_speed;
	o->active = 1;
	o->heading = 0;
	o->steer_angle = o->steer_rate = 0;
	o->push_x = o->push_z = o->offset_x = o->offset_z = 0.0f;
	o->field_18 = cos_table[o->heading & ANGLE_MASK];
	o->field_1c = 0;
	o->field_20 = sin_table[o->heading & ANGLE_MASK];
	/* The original tests the kind before setting it, so this is 0.2 for every car. */
	o->acceleration = o->field_e4 == DRIVER_RIVAL ? 0.2f - (float)(rand() % 5) / 10.0f : 0.2f;
	o->mass = ct->mass;
	ob->texture = ct->texture_id;
	matrix_t identity;
	matrix_identity(&identity);
	set_object3d_matrix(object, &identity);
	ob->update = update_opponent_object;
	ob->field_36 = 1;
	ob->visible = 1;
	ob->opponent = op;
	ob->update_interval = 1;
	build_object_execute_buffer(object);
	compute_object_bounds(object);
	o->field_e0 = get_new_driver();
	o->field_e4 = kind;
	return object;
}

/* The point `lane` across and `back` behind a chunk's centre. */
static void lane_position(opponent_t *o, int chunk, int lane, float along)
{
	const weg_chunk_t *ch = &weg_chunks[chunk];
	const vertex3d_t *p = &vertices3d[weg_sections[ch->section].point];
	float l = (float)(int)lane_offsets[lane];
	o->x = ch->side[0] * l + p->x;
	o->y = ch->side[1] * l + p->y;
	o->z = ch->side[2] * l + p->z;
	o->x -= ch->side[2] * along;
	o->z += along * ch->side[0];
}

/* next_opponent_waypoint: the chunk `ahead` chunks on; past the track's end the car
 * starts again from the other end. */
static void next_opponent_waypoint(int op, int ahead)
{
	opponent_t *o = &opponent_data[op];
	int restart = -1;
	if (o->waypoint < 0) {
		if (o->prev_waypoint < 0) {
			fprintf(stderr, "illegal waypoints for opponent %d\n", op);
			exit(1);
		}
		o->waypoint = o->prev_waypoint;
		return;
	}
	o->prev_waypoint = o->waypoint;
	if (o->direction == 1) {
		o->waypoint = o->chunk + ahead;
		if (o->waypoint <= weg_chunk_count - 1)
			return;
		o->chunk = 3;
		o->waypoint = 5;
		restart = o->chunk;
		o->heading = weg_sections[o->chunk].heading - 0x100;
	}
	else {
		o->waypoint = o->chunk - ahead;
		if (o->waypoint >= 0)
			return;
		o->chunk = weg_chunk_count - 4;
		o->waypoint = weg_chunk_count - 6;
		restart = o->chunk;
		o->heading = weg_sections[o->chunk].heading + 0x100;
	}
	lane_position(o, restart, o->lane, -40.0f);
	o->offset_x = o->offset_z = 0.0f;
	o->push_x = o->push_z = 0.0f;
	o->heading_delta = 0;
}

/* compute_opponent_target: steer toward the target lane at the waypoint. */
static void compute_opponent_target(int op, float target[3])
{
	opponent_t *o = &opponent_data[op];
	const weg_chunk_t *ch = &weg_chunks[o->waypoint];
	const vertex3d_t *p = &vertices3d[weg_sections[ch->section].point];
	float l = (float)(int)lane_offsets[o->target_lane];
	o->target_x = ch->side[0] * l + p->x;
	o->target_y = ch->side[1] * l + p->y;
	o->target_z = l * ch->side[2] + p->z;
	float t[3] = { o->target_x, o->target_y, o->target_z }, pos[3] = { o->x, o->y, o->z };
	float dir[3];
	vector_subtract(dir, t, pos);
	normalize_vector(dir);
	/* cross_product_y with the car's direction (field_18, field_1c, field_20). */
	float c = dir[0] * o->field_20 - dir[2] * o->field_18;
	int turn = (int)(fabsf(c) * 32.0f * 1.5f);
	o->heading_delta = c < 0.0f ? turn : -turn;
	target[0] = o->target_x;
	target[1] = o->target_y;
	target[2] = o->target_z;
}

static void move_opponent_toward_target(int op)
{
	float t[3];
	compute_opponent_target(op, t);
	opponent_t *o = &opponent_data[op];
	if (o->speed < o->max_speed) {
		o->speed += o->acceleration;
		if (o->max_speed < o->speed)
			o->speed = o->max_speed;
	}
	else {
		o->speed = o->max_speed;
	}
	o->distance_to_target = fabsf(o->z - t[2]) + fabsf(o->x - t[0]) + fabsf(o->y - t[1]);
}

static void evaluate_opponent(int op)
{
	opponent_t *o = &opponent_data[op];
	if (!o->active)
		return;
	/* TODO: the car's engine sound, nearer than 500 (update_object_sound). */
	move_opponent_toward_target(op);
	o->steer_angle += o->steer_rate;
	o->steer_angle = (int)(o->steer_angle * 0.95f);
	o->steer_rate = (int)(o->steer_rate * 0.95f);
	o->push_x *= 0.75f;
	o->push_z *= 0.75f;
}

/* place_opponent: a racer or police car on a chunk and lane. */
void place_opponent(int op, int direction, int chunk, int lane, float along)
{
	opponent_t *o = &opponent_data[op];
	o->direction = direction;
	o->target_lane = o->lane = lane;
	lane_position(o, chunk, lane, along + 40.0f);
	o->chunk = chunk;
	next_opponent_waypoint(op, 2);
	set_map_occupant(op, chunk, lane);
	o->prev_waypoint = direction == 0 ? (chunk + 1) % (weg_chunk_count - 1) : chunk - 1;
	if (o->field_e0 != -1)
		assign_driver_to_opponent(o->field_e0, op, o->field_e4);
	o->heading = weg_sections[chunk].heading + (direction == 0 ? 0x100 : -0x100);
	o->speed = 0.0f;
	o->field_18 = 0.0f;
	o->field_1c = 0;
	o->field_20 = 0.0f;
	o->heading_delta = 0;
	o->max_speed = 0.0f;
}

/* scatter_traffic: every car from `first` on at a random free place, mostly driving
 * the stage's way, not within `margin` chunks of the ends. */
void scatter_traffic(int first, int margin)
{
	for (int i = first; i < opponents_used; i++) {
		opponent_t *o = &opponent_data[i];
		int chunk, lane, direction;
		do {
			direction = rand() % 4 == 0 ? track_info[track_index].direction == 0
				: track_info[track_index].direction;
			chunk = margin + rand() % (weg_chunk_count - margin * 3);
			int r = rand();
			lane = direction == 1 ? r % 2 + 2 : r % 2 + 7;
		} while (get_map_occupant(chunk, lane) != -1);
		set_map_occupant(i, chunk, lane);
		o->chunk = o->waypoint = chunk;
		o->target_lane = o->lane = lane;
		o->direction = direction;
		/* One unit back along the road, as the original. */
		lane_position(o, chunk, lane, 1.0f);
		o->target_x = o->x;
		o->target_y = o->y;
		o->target_z = o->z;
		o->speed = 0.0f;
		o->prev_waypoint = direction == 0 ? chunk + 1 : chunk - 1;
		if (o->field_e0 != -1)
			assign_driver_to_opponent(o->field_e0, i, o->field_e4);
		o->heading = weg_sections[chunk].heading + (direction == 0 ? 0x100 : -0x100);
		o->field_18 = 0.0f;
		o->field_1c = 0;
		o->field_20 = 0.0f;
		o->heading_delta = 0;
	}
}

void mark_active_racers(void)
{
	for (int i = 0; i < opponent_count; i++) {
		int ob = opponent_data[i].object;
		if (opponent_data[i].active && objects3d[ob].in_use && ob != player_index)
			objects3d[ob].update_interval = 1;
	}
}

/* --- the active list (active.cpp) ------------------------------------------------ */

void init_active_opponent_list(int size)
{
	free(active_opponent_list);
	free(nearby_opponent_list);
	active_opponent_list = calloc((size_t)size, sizeof(int));
	nearby_opponent_list = calloc((size_t)size, sizeof(int));
	active_list_size = size;
}

/* The six racers always; other cars nearer than `active` chunks to the player move,
 * nearer than `nearby` can be hit. */
static void update_active_opponent_list(int active, int nearby)
{
	int player = opponent_data[0].chunk, n = 0, m = 0;
	for (int i = 0; i < 6 && i < active_list_size; i++) {
		active_opponent_list[n++] = i;
		nearby_opponent_list[m++] = i;
	}
	for (int i = 6; i < active_list_size && i < opponent_count; i++) {
		if (!opponent_data[i].active)
			continue;
		int d = abs(opponent_data[i].chunk - player);
		if (d < active)
			active_opponent_list[n++] = i;
		else if (nearby <= d)
			continue;
		nearby_opponent_list[m++] = i;
	}
	active_opponent_count = n;
	nearby_opponent_count = m;
}

/* --- collisions (objio.cpp) ------------------------------------------------------ */

static float cross_y(const float a[3], const float b[3])
{
	return a[0] * b[2] - a[2] * b[0];
}

/* Whether any of the five points (corners and centre) is inside the quad c[0..3]. */
static int points_in_quad(const float pts[5][3], const float c[4][3])
{
	float e0[3], e1[3], e2[3], e3[3], d[3];
	vector_subtract(e0, c[1], c[0]);
	vector_subtract(e1, c[2], c[1]);
	vector_subtract(e2, c[3], c[2]);
	vector_subtract(e3, c[0], c[3]);
	for (int i = 0; i < 5; i++) {
		vector_subtract(d, pts[i], c[0]);
		if (cross_y(e0, d) < 0.0f)
			continue;
		vector_subtract(d, pts[i], c[2]);
		if (cross_y(e2, d) < 0.0f)
			continue;
		vector_subtract(d, pts[i], c[1]);
		if (cross_y(e1, d) < 0.0f)
			continue;
		vector_subtract(d, pts[i], c[3]);
		if (cross_y(e3, d) < 0.0f)
			continue;
		return 1;
	}
	return 0;
}

static void resolve_collision(int object_a, int object_b);

/* racers_collide: 0 apart, 1 within each other's radius, 2 overlapping (then, with
 * `resolve`, pushed apart). */
static int racers_collide(int object_a, int object_b, int resolve)
{
	const opponent_t *a = &opponent_data[objects3d[object_a].opponent];
	const opponent_t *b = &opponent_data[objects3d[object_b].opponent];
	float dx = b->x - (a->x + a->offset_x), dz = b->z - (a->z + a->offset_z);
	float r = a->radius + b->radius;
	if (dz * dz + dx * dx > r * r)
		return 0;
	float pa[5][3], pb[5][3];
	for (int i = 0; i < 4; i++) {
		pa[i][0] = a->world_corners[i * 3] + a->offset_x;
		pa[i][1] = 0.0f;
		pa[i][2] = a->world_corners[i * 3 + 2] + a->offset_z;
		pb[i][0] = b->world_corners[i * 3];
		pb[i][1] = 0.0f;
		pb[i][2] = b->world_corners[i * 3 + 2];
	}
	pa[4][0] = a->x + a->offset_x;
	pa[4][1] = 0.0f;
	pa[4][2] = a->z + a->offset_z;
	pb[4][0] = b->x;
	pb[4][1] = 0.0f;
	pb[4][2] = b->z;
	if (points_in_quad(pb, pa) || points_in_quad(pa, pb)) {
		if (resolve)
			resolve_collision(object_a, object_b);
		return 2;
	}
	return 1;
}

/* The two cars exchange momentum by mass; then A's move is cut short until clear. */
static void resolve_collision(int object_a, int object_b)
{
	opponent_t *a = &opponent_data[objects3d[object_a].opponent];
	opponent_t *b = &opponent_data[objects3d[object_b].opponent];
	float ax = a->offset_x, az = a->offset_z, m = a->mass + b->mass;
	float cx = (b->mass * b->offset_x + a->mass * ax) / m;
	float cz = (b->mass * b->offset_z + a->mass * az) / m;
	float pax = (cx - ax) + cx, paz = (cz - az) + cz;
	float pbx = (cx - b->offset_x) + cx, pbz = (cz - b->offset_z) + cz;
	a->push_x += pax * 0.6f;
	a->push_z += paz * 0.6f;
	if (10.0f < fabsf(a->push_x) || 10.0f < fabsf(a->push_z)) {
		a->push_x -= pax * 0.6f;
		a->push_z -= paz * 0.6f;
	}
	b->push_x += pbx * 0.6f;
	b->push_z += pbz * 0.6f;
	if (10.0f < fabsf(b->push_x) || 10.0f < fabsf(b->push_z)) {
		b->push_x -= pbx * 0.6f;
		b->push_z -= pbz * 0.6f;
	}
	if (racers_collide(object_a, object_b, 0)) {
		float f;
		for (f = 0.1f; f < 1.0f; f += 0.1f) {
			a->offset_x = f * ax;
			a->offset_z = f * az;
			if (racers_collide(object_a, object_b, 0))
				break;
		}
		a->offset_x = (f - 0.1f) * ax;
		a->offset_z = (f - 0.1f) * az;
		a->speed *= 0.7f;
		a->push_x *= 0.8f;
		a->push_z *= 0.8f;
		play_crash_sound();
	}
}

static int check_opponent_collisions(int object, int op)
{
	for (int i = 0; i < nearby_opponent_count; i++) {
		int k = nearby_opponent_list[i];
		if (k == op)
			continue;
		if (abs(opponent_data[k].chunk - opponent_data[op].chunk) < 2) {
			update_racer_corners(k);
			if (racers_collide(object, opponent_data[k].object, 0))
				return 1;
		}
	}
	return 0;
}

/* --- every frame ----------------------------------------------------------------- */

static void update_all_opponents(void)
{
	for (int i = 1; i < active_opponent_count; i++) {
		int op = active_opponent_list[i];
		if (opponent_data[op].car_type != 0x10 && (race_running || 0xd < op))
			drive_opponent(opponent_data[op].field_e0);
	}
	for (int j = 1; j < active_opponent_count; j++) {
		/* The original checks the car type of opponent j, not of the j-th active car. */
		if (opponent_data[j].car_type == 0x10)
			continue;
		int op = active_opponent_list[j];
		opponent_t *o = &opponent_data[op];
		clear_map_occupant(o->chunk, o->lane);
		evaluate_opponent(op);
		o->chunk = update_chunk_for_position(o->chunk, o->x, o->z);
		next_opponent_waypoint(op, 2);
		o->lane = get_racer_lane(op);
		set_map_occupant(op, o->chunk, o->lane);
		if (0 < op && op < 6) {
			/* Rivals keep near the player: slower when far ahead, faster behind. */
			driver_t *d = &driver_list[o->field_e0];
			int player_place = get_racer_entry(0), place = get_racer_entry(op);
			if (place < player_place) {
				if (racer_range < abs(o->chunk - opponent_data[0].chunk))
					d->speed = racer_low_speed;
			}
			else if (player_place < place) {
				d->speed = racer_high_speed;
			}
			rival_state_t *rs = &rival_states[op - 1];
			int over = -1;
			if (o->direction == 1 && weg_chunk_count - 0xf <= o->chunk)
				over = o->chunk - (weg_chunk_count - 0xf);
			else if (o->direction != 1 && o->chunk < 0x10)
				over = 0xf - o->chunk;
			if (over >= 0) {
				rs->counter = 10 - over;
				set_racer_finish_time(op);
				rs->state = 4;
			}
		}
	}
}

void update_racers(void)
{
	int heading = player_heading;
	int pl = objects3d[player_index].opponent;
	opponent_t *p = &opponent_data[pl];
	clear_map_occupant(p->chunk, p->lane);
	handle_player_controls();
	p->x = player_x;
	p->y = player_y;
	p->z = player_z;
	p->heading = heading;
	p->heading_delta = player_heading - heading;
	p->speed = player_speed;
	p->chunk = player_chunk;
	p->lane = get_racer_lane(pl);
	set_map_occupant(pl, p->chunk, p->lane);
	update_active_opponent_list(10, 0xc);
	update_all_opponents();

	/* Each active car turns and gets its move for this frame. */
	for (int i = 0; i < active_opponent_count; i++) {
		int op = active_opponent_list[i];
		opponent_t *o = &opponent_data[op];
		o->heading += o->heading_delta;
		update_racer_corners(op);
		if (o->heading_delta != 0 &&
			(op == pl || abs(o->chunk - opponent_data[pl].chunk) <= 1)) {
			o->offset_x = o->offset_z = 0.0f;
			if (o->speed < 4.0f) {
				/* Slow cars may not turn into another car: undo the turn in quarters. */
				int was = o->heading, k = 4;
				while (--k != -1) {
					if (!check_opponent_collisions(o->object, op))
						break;
					o->heading -= o->heading_delta / 4;
					update_racer_corners(op);
				}
				if (k == 0) {
					o->heading = was;
					update_racer_corners(op);
				}
				o->heading_delta = 0;
			}
		}
		o->field_18 = cos_table[(o->heading + 0x100) & ANGLE_MASK];
		o->field_1c = 0;
		o->field_20 = sin_table[(o->heading + 0x100) & ANGLE_MASK];
		o->offset_x = o->field_18 * o->speed + o->push_x;
		o->offset_z = o->field_20 * o->speed + o->push_z;
		collide_racer_with_road_edge(op);
		if (op == 0)
			collide_player_with_track_ends();
	}
	/* Then everyone moves, pushing apart what they run into. */
	for (int i = 0; i < active_opponent_count; i++) {
		int op = active_opponent_list[i];
		opponent_t *o = &opponent_data[op];
		if (!objects3d[o->object].in_use || !o->active)
			continue;
		update_racer_corners(op);
		for (int j = 0; j < nearby_opponent_count; j++) {
			int k = nearby_opponent_list[j];
			if (k != op && o->chunk - opponent_data[k].chunk < 2) {
				update_racer_corners(k);
				racers_collide(o->object, opponent_data[k].object, 1);
			}
		}
		o->x += o->offset_x;
		o->z += o->offset_z;
		update_racer_corners(op);
	}
	p->push_x *= 0.75f;
	p->push_z *= 0.75f;
	player_x = p->x;
	player_z = p->z;
	player_y = p->y;
	player_speed = p->speed;
	player_heading = p->heading;
	select_rpm_step_for_speed(player_speed);
	update_camera();
}
