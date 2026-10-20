#include "race/driver.h"

#include <stdio.h>
#include <stdlib.h>

#include "race/object3d.h"
#include "race/opponent.h"
#include "race/race.h"
#include "race/weg.h"

driver_t *driver_list;
rival_state_t *rival_states;
static police_state_t *police_states;
static int driver_count, police_state_count;
float racer_high_speed = 16.0f, racer_low_speed = 13.0f;
int racer_range = 2;

enum { MAP_LANES = 11 };
static int (*opponent_map)[MAP_LANES + 1];
static int map_chunks;

/* --- the road map ---------------------------------------------------------------- */

void init_opponent_map(void)
{
	map_chunks = weg_chunk_count;
	opponent_map = malloc((size_t)map_chunks * sizeof *opponent_map);
	for (int i = 0; i < map_chunks; i++) {
		for (int l = 0; l <= MAP_LANES; l++)
			opponent_map[i][l] = -1;
	}
}

void exit_opponent_map(void)
{
	free(opponent_map);
	opponent_map = NULL;
	map_chunks = 0;
}

static int map_ok(int chunk, int lane)
{
	return opponent_map && chunk >= 0 && chunk < map_chunks && lane >= 0 && lane <= MAP_LANES;
}

void set_map_occupant(int opponent, int chunk, int lane)
{
	if (map_ok(chunk, lane))
		opponent_map[chunk][lane] = opponent;
}

int get_map_occupant(int chunk, int lane)
{
	return map_ok(chunk, lane) ? opponent_map[chunk][lane] : -1;
}

void clear_map_occupant(int chunk, int lane)
{
	if (map_ok(chunk, lane))
		opponent_map[chunk][lane] = -1;
}

/* --- drivers --------------------------------------------------------------------- */

void init_drivers(int count, int rivals, int police)
{
	driver_list = calloc((size_t)count, sizeof *driver_list);
	rival_states = calloc((size_t)rivals, sizeof *rival_states);
	police_states = calloc((size_t)police + 1, sizeof *police_states);
	driver_count = count;
	police_state_count = police;
	for (int i = 0; i < rivals; i++) {
		rival_states[i].state = rand() % 2;
		rival_states[i].stopped_by = -1;
		rival_states[i].counter = 0;
	}
}

void exit_drivers(void)
{
	free(driver_list);
	free(rival_states);
	free(police_states);
	driver_list = NULL;
	rival_states = NULL;
	police_states = NULL;
	driver_count = police_state_count = 0;
}

int get_new_driver(void)
{
	for (int i = 0; i < driver_count; i++) {
		if (!driver_list[i].used) {
			driver_list[i].used = 1;
			return i;
		}
	}
	fprintf(stderr, "not enough drivers available\n");
	exit(1);
}

void assign_driver_to_opponent(int driver, int opponent, int kind)
{
	driver_t *d = &driver_list[driver];
	if (!d->used || d->assigned) {
		fprintf(stderr, "cannot assign driver %d to opponent %d\n", driver, opponent);
		exit(1);
	}
	d->opponent = opponent;
	/* The lane a driver keeps to: the second (rivals) or third lane out. */
	if (opponent_data[opponent].direction == 1)
		d->lane = kind == DRIVER_RIVAL ? 1 : 2;
	else
		d->lane = kind == DRIVER_RIVAL ? 9 : 8;
	d->speed = opponent_data[opponent].max_speed;
	d->assigned = 1;
	d->kind = kind;
	if (kind == DRIVER_CIVILIAN)
		d->index = -1;
	else if (kind == DRIVER_RIVAL)
		d->index = opponent - 1;
	else if (kind == DRIVER_POLICE)
		d->index = opponent - 6;
}

static int wrap_chunk(int c)
{
	while (c < 0)
		c += weg_chunk_count;
	while (weg_chunk_count <= c)
		c -= weg_chunk_count;
	return c;
}

/* scan_lanes_ahead: per lane the first car `ahead` chunks forward and `behind` back. */
static void scan_lanes_ahead(int front[MAP_LANES], int back[MAP_LANES], int self, int chunk,
	int ahead, int behind, int direction)
{
	int step = direction == 0 ? -1 : 1;
	for (int l = 0; l < MAP_LANES; l++)
		front[l] = back[l] = -1;
	for (int i = 0, c = chunk; i <= ahead; i++, c += step) {
		for (int l = 0; l < MAP_LANES; l++) {
			if (front[l] == -1) {
				front[l] = get_map_occupant(wrap_chunk(c), l);
				if (front[l] == self)
					front[l] = -1;
			}
		}
	}
	for (int i = 0, c = chunk; i <= behind; i++, c -= step) {
		for (int l = 0; l < MAP_LANES; l++) {
			if (back[l] == -1) {
				back[l] = get_map_occupant(wrap_chunk(c), l);
				if (back[l] == self)
					back[l] = -1;
			}
		}
	}
}

/* scan_lanes_for_heading: which racers are on this side of the road near the chunk. */
static void scan_lanes_for_heading(int chunk, int range, int direction, int seen[6])
{
	int step = direction == 0 ? -1 : 1, first = direction == 0 ? 6 : 0;
	int last = direction == 0 ? 10 : 4;
	for (int i = 0; i < 6; i++)
		seen[i] = 0;
	for (int i = 0, c = chunk; i <= range; i++, c += step) {
		for (int l = first; l <= last; l++) {
			int o = get_map_occupant(wrap_chunk(c), l);
			if (o >= 0 && o < 6)
				seen[o] = 1;
		}
	}
}

/* racer_distance_margin: squared distance, less the radii (not squared: as the
 * original). */
static float racer_distance_margin(const opponent_t *a, const opponent_t *b)
{
	float dx = a->x - b->x, dz = a->z - b->z;
	return dx * dx + dz * dz - (a->radius + b->radius);
}

/* How good a lane is: 0 someone close behind in it, 1 a car ahead within 240, 2 within
 * 320, 3 further, 4 nothing ahead. *occupant: the car ahead, if any. */
static int classify_lane_occupant(const opponent_t *o, const int front[MAP_LANES],
	const int back[MAP_LANES], int lane, const opponent_t **occupant)
{
	*occupant = NULL;
	if (lane < 0 || lane >= MAP_LANES)
		return 0;
	if (lane != o->lane && back[lane] != -1)
		return 0;
	if (front[lane] == -1)
		return 4;
	*occupant = &opponent_data[front[lane]];
	float d = racer_distance_margin(*occupant, o);
	if (57600.0f < d)
		return 102400.0f < d ? 3 : 2;
	return 1;
}

/* The lanes either side: toward the median ("faster") and toward the edge ("slower"),
 * only one of them at the outer or inner lane. Returns how many there are. */
static int rate_lanes(const opponent_t *o, const int front[MAP_LANES], const int back[MAP_LANES],
	int lanes[3], int rating[3], const opponent_t *occupant[3])
{
	int d = o->direction == 1 ? 1 : -1;
	int outer = o->direction == 1 ? 0 : 10, inner = o->direction == 1 ? 3 : 7;
	lanes[0] = o->lane;
	lanes[1] = o->lane + d;
	lanes[2] = o->lane - d;
	rating[0] = classify_lane_occupant(o, front, back, lanes[0], &occupant[0]);
	if (o->lane == inner || o->lane == outer) {
		if (o->lane == inner)
			lanes[1] = lanes[2];
		rating[1] = classify_lane_occupant(o, front, back, lanes[1], &occupant[1]);
		return 2;
	}
	rating[1] = classify_lane_occupant(o, front, back, lanes[1], &occupant[1]);
	rating[2] = classify_lane_occupant(o, front, back, lanes[2], &occupant[2]);
	return 3;
}

/* The lane choice rivals and chasing police share: the best rated lane, on a tie the
 * one nearer the preferred lane. Full speed if the chosen lane has room, else the speed
 * of the car ahead in it. */
static void choose_lane(opponent_t *o, const driver_t *d, int n, const int lanes[3],
	const int rating[3], const opponent_t *occupant[3])
{
	int pick = 0;
	if (n == 2) {
		if (rating[1] == rating[0]) {
			if (rating[1] >= 2 && abs(d->lane - lanes[1]) < abs(d->lane - lanes[0]))
				pick = 1;
		}
		else if (rating[0] < rating[1]) {
			pick = 1;
		}
	}
	else if (rating[0] < rating[1] || rating[0] < rating[2]) {
		if (rating[2] == rating[1])
			pick = abs(d->lane - lanes[1]) > abs(d->lane - lanes[2]) ? 2 : 1;
		else
			pick = rating[1] < rating[2] ? 2 : 1;
	}
	else if (rating[0] == rating[1]) {
		if (abs(d->lane - lanes[1]) < abs(d->lane - lanes[0]))
			pick = 1;
	}
	else if (rating[2] == rating[0] && abs(d->lane - lanes[0]) > abs(d->lane - lanes[2])) {
		pick = 2;
	}
	if (pick)
		o->target_lane = lanes[pick];
	if (1 < rating[pick] || !occupant[pick])
		o->max_speed = d->speed;
	else
		o->max_speed = occupant[pick]->speed;
}

/* drive_traffic in the recovered names: a rival's driving. */
static void drive_rival(int driver)
{
	driver_t *d = &driver_list[driver];
	opponent_t *o = &opponent_data[d->opponent];
	int front[MAP_LANES], back[MAP_LANES], lanes[3], rating[3];
	const opponent_t *occupant[3] = { NULL, NULL, NULL };
	scan_lanes_ahead(front, back, d->opponent, o->chunk, 9, 3, o->direction);
	int n = rate_lanes(o, front, back, lanes, rating, occupant);
	rival_state_t *rs = &rival_states[d->index];
	if (rs->stopped_by != -1) {
		/* Stopped by the police: brake to 2 and wait. */
		if (o->max_speed != 2.0f) {
			o->max_speed *= 0.5f;
			if (o->max_speed < 2.0f)
				o->max_speed = 2.0f;
			return;
		}
		if (rs->counter == 0)
			return;
		if (--rs->counter == 0)
			rs->stopped_by = -1;
		return;
	}
	if (rs->state == 4) {
		/* Finished: to the outer lane, slowing to a stop. */
		d->lane = o->direction == 1 ? 0 : 10;
		d->speed = (float)rs->counter;
		if (o->max_speed == 0.0f)
			objects3d[o->object].update_interval = -1;
	}
	choose_lane(o, d, n, lanes, rating, occupant);
}

/* drive_racer in the recovered names: a police car. */
static void drive_police(int driver)
{
	driver_t *d = &driver_list[driver];
	opponent_t *o = &opponent_data[d->opponent];
	police_state_t *p = &police_states[d->index];
	int step = o->direction == 1 ? 1 : -1, outer = o->direction == 1 ? 0 : 10;
	if (p->state == 0) {
		/* Parked: watch for a racer speeding past. */
		int seen[6];
		o->max_speed = 0.0f;
		scan_lanes_for_heading(o->chunk, 6, o->direction, seen);
		for (int k = 0; k < 6; k++) {
			if (!seen[k])
				continue;
			p->fast = opponent_data[k].speed > 9.0f;
			if (!p->fast)
				continue;
			/* "At most two cars on a racer" - but the original compares each police car's
			 * target with that car's own number, not with the racer; kept. */
			int chasing = 0;
			for (int j = 0; j < police_state_count; j++) {
				if (police_states[j].target == j)
					chasing++;
			}
			if (chasing >= 2)
				continue;
			/* The player two times in three; a rival only by an even-numbered car, very
			 * rarely (the original's rand() == 0), during the race. */
			int chase = k == 0 ? rand() % 3 != 0
				: d->index % 2 == 0 && (rand() == 0) % 3 != 0 && race_state == RACE_RUNNING;
			if (chase) {
				p->state = 1;
				p->target = k;
				p->speed = (int)d->speed;
				p->lane = d->lane;
				o->max_speed = d->speed;
				return;
			}
		}
		return;
	}
	int front[MAP_LANES], back[MAP_LANES], lanes[3], rating[3];
	const opponent_t *occupant[3] = { NULL, NULL, NULL };
	scan_lanes_ahead(front, back, d->opponent, o->chunk, 9, 3, o->direction);
	if (p->state == 1) {
		const opponent_t *t = &opponent_data[p->target];
		if (t->chunk == o->chunk || (step * t->chunk > step * o->chunk)) {
			/* Behind the racer: chase in its lane. */
			d->speed = (float)p->speed;
			d->lane = t->lane;
		}
		else if (t->lane == o->lane) {
			/* In front of it in its lane: brake, and stop it when close. */
			d->speed = (float)(int)(t->speed - 1.0f);
			if (d->speed < 0.0f)
				d->speed = 0.0f;
			d->lane = t->lane;
			if (abs(o->chunk - t->chunk) < 2) {
				if (p->target == 0) {
					set_race_state(RACE_FINE);
				}
				else {
					rival_state_t *rs = &rival_states[driver_list[t->field_e0].index];
					rs->stopped_by = d->opponent;
					rs->counter = 100;
				}
				int target = p->target;
				for (int j = 0; j < police_state_count; j++) {
					if (police_states[j].target == target) {
						police_states[j].state = 2;
						police_states[j].target = -1;
						police_states[j].counter = 350;
					}
				}
			}
		}
		else {
			/* Alongside: move into its lane at its speed. */
			o->target_lane = t->lane;
			o->max_speed = t->speed < 0.0f ? 0.0f : t->speed;
			return;
		}
	}
	int n = rate_lanes(o, front, back, lanes, rating, occupant);
	if (p->state == 2) {
		/* Back to the hard shoulder and park. */
		if (o->lane != outer) {
			o->target_lane = outer;
			p->counter = 350;
			return;
		}
		if (p->counter != 0) {
			o->max_speed *= 0.99f;
			p->counter--;
			if (rating[0] < 2 && occupant[0])
				o->max_speed = occupant[0]->speed;
			return;
		}
		o->max_speed = 0.0f;
		p->state = 0;
		return;
	}
	choose_lane(o, d, n, lanes, rating, occupant);
}

/* Civilians (drive_opponent's own case). */
static void drive_civilian(driver_t *d)
{
	opponent_t *o = &opponent_data[d->opponent];
	int front[MAP_LANES], back[MAP_LANES];
	int lane = o->lane, step = o->direction == 1 ? 1 : -1;
	scan_lanes_ahead(front, back, d->opponent, o->chunk, 9, 3, o->direction);
	/* Off the road (lane -1) the original read outside its lane arrays; the port keeps
	 * the car at its speed until it is back in a lane. */
	if (lane < 0 || lane >= MAP_LANES) {
		o->max_speed = d->speed;
		return;
	}
	if (d->lane != lane) {
		/* Out of its lane (overtaking): back when there is room. */
		int other = lane - step;
		if (other < 0 || other >= MAP_LANES)
			other = lane;
		if (front[lane] == -1) {
			if (back[other] != -1) {
				o->max_speed = d->speed;
				return;
			}
			if (front[other] == -1) {
				o->target_lane = other;
				o->max_speed = d->speed;
				return;
			}
			const opponent_t *o2 = &opponent_data[front[other]];
			if (o2->speed < d->speed) {
				o->max_speed = d->speed; /* faster than the slow lane */
				return;
			}
			float dist = racer_distance_margin(o2, o);
			if (57600.0f < dist) {
				o->target_lane = other;
				if (102400.0f <= dist) {
					o->max_speed = d->speed;
				}
				else {
					o->max_speed = o2->speed < d->speed ? o2->speed : d->speed;
					if (o->max_speed < 0.0f)
						o->max_speed = 0.0f;
				}
				return;
			}
			o->max_speed = d->speed;
			return;
		}
		const opponent_t *o2 = &opponent_data[front[lane]];
		float dist = racer_distance_margin(o2, o);
		if (57600.0f < dist) {
			if (102400.0f <= dist) {
				o->max_speed = d->speed;
				return;
			}
			if (o2->speed < d->speed) {
				o->max_speed = o2->speed < 0.0f ? 0.0f : o2->speed;
				return;
			}
			if (back[other] == -1) {
				if (front[other] == -1) {
					o->max_speed = d->speed;
					o->target_lane = other;
					return;
				}
				const opponent_t *o4 = &opponent_data[front[other]];
				if (o4->speed < d->speed) {
					o->max_speed = d->speed;
					return;
				}
				float d4 = racer_distance_margin(o4, o);
				if (57600.0f < d4) {
					o->target_lane = other;
					if (d4 < 102400.0f) {
						o->max_speed = o4->speed < d->speed ? o4->speed : d->speed;
						if (o->max_speed < 0.0f)
							o->max_speed = 0.0f;
					}
					else {
						o->max_speed = d->speed;
					}
					return;
				}
			}
		}
		else if (back[other] == -1) {
			if (front[other] == -1) {
				o->max_speed = d->speed;
				o->target_lane = other;
				return;
			}
			const opponent_t *o3 = &opponent_data[front[other]];
			o->max_speed = o3->speed < 0.0f ? 0.0f : o3->speed;
			o->target_lane = other;
			return;
		}
		/* Follow the car ahead. */
		o->max_speed = o2->speed < d->speed ? o2->speed : d->speed;
		if (o->max_speed < 0.0f)
			o->max_speed = 0.0f;
		return;
	}
	if (front[lane] == -1) {
		o->max_speed = d->speed; /* nothing in sight */
		return;
	}
	const opponent_t *o2 = &opponent_data[front[lane]];
	float dist = racer_distance_margin(o2, o);
	if (dist <= 57600.0f) {
		int other = lane + step;
		if (d->speed <= o2->speed || o->speed < o2->speed || other < 0 || other >= MAP_LANES) {
			/* Slowing down behind it. */
			o->max_speed = o2->speed < d->speed ? o2->speed : d->speed;
			if (o->max_speed < 0.0f)
				o->max_speed = 0.0f;
			return;
		}
		if (back[other] == -1) {
			/* Overtaking. */
			o->target_lane = other;
			o->max_speed = d->speed;
			return;
		}
		o->max_speed = o2->speed < 0.0f ? 0.0f : o2->speed;
		return;
	}
	if (102400.0f <= dist) {
		o->max_speed = d->speed;
		return;
	}
	/* Following, or getting ready to overtake. */
	if (d->speed <= o2->speed) {
		o->max_speed = o2->speed < d->speed ? o2->speed : d->speed;
		if (o->max_speed < 0.0f)
			o->max_speed = 0.0f;
		return;
	}
	o->max_speed = d->speed;
}

void drive_opponent(int driver)
{
	driver_t *d = &driver_list[driver];
	if (!d->used || !d->assigned) {
		fprintf(stderr, "cannot drive driver %d\n", driver);
		exit(1);
	}
	switch (d->kind) {
	case 0:
	case 1:
		break;
	case DRIVER_CIVILIAN:
		drive_civilian(d);
		break;
	case DRIVER_RIVAL:
		drive_rival(driver);
		break;
	case DRIVER_POLICE:
		drive_police(driver);
		break;
	default:
		fprintf(stderr, "illegal driver type %d\n", d->kind);
		exit(1);
	}
}
