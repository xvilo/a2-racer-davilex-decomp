#include "race/camera.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "race/mapexec.h"
#include "race/math3d.h"
#include "race/object3d.h"
#include "race/opponent.h"
#include "race/race.h"
#include "race/render.h"
#include "race/weg.h"

float camera_height = 15.0f, camera_distance = -31.0f;
int camera_position = CAMERA_CHASE;
int watched_opponent;
int car_pitch, car_yaw, car_roll;
int look_offset;

static float camera_x, camera_y, camera_z;
static float camera_yaw, camera_pitch;
static float camera_tilt, camera_roll; /* the road's slope under the car, smoothed */
static float camera_lag_xz, camera_lag_y;
static float car_height_offset; /* FLOAT_0045b264: 0 */

/* init_atan_table(4, 0, 1000, 1, -61, 256): the camera's downward pitch for a height
 * above the car, looking at a point 61 ahead. */
static int atan_table[1000];

void init_cameras(void)
{
	camera_x = camera_y = camera_z = 0.0f;
	camera_yaw = camera_pitch = 0.0f;
	camera_tilt = camera_roll = 0.0f;
	camera_lag_xz = camera_lag_y = 0.0f;
	watched_opponent = look_offset = 0;
	car_pitch = car_yaw = car_roll = 0;
	for (int j = 0; j < 1000; j++) {
		double a = atan2((float)(0 + 1 * j), -61.0);
		atan_table[j] = (int)(-(((1.570798 - a) * 256.0) / 1.570798 + 256.0) + 0.5);
	}
}

static int atan_lookup(float height)
{
	int i = (int)(height + 0.5f) - 0;
	if (i < 0)
		i = 0;
	if (i >= 1000)
		i = 999;
	return atan_table[i];
}

void set_camera(int camera, float x, float y, float z, int pitch, int yaw, int roll)
{
	(void)camera;
	matrix_t r, t;
	matrix_identity(&r);
	rotate_matrix_x(&r, pitch);
	rotate_matrix_z(&r, roll);
	rotate_matrix_y(&r, yaw);
	matrix_translation(&t, x, y, z);
	set_view(&r, &t);
}

void set_camera_position(int mode)
{
	switch (mode) {
	case CAMERA_CHASE:
		objects3d[player_index].visible = 1;
		camera_distance = -31.0f;
		camera_height = 15.0f;
		break;
	case CAMERA_COCKPIT:
		objects3d[player_index].visible = 0;
		camera_distance = -4.0f;
		camera_height = 7.0f;
		break;
	case CAMERA_OPPONENT:
		camera_distance = -31.0f;
		camera_height = 32.0f;
		break;
	case CAMERA_OVERHEAD:
		objects3d[player_index].visible = 1;
		camera_height = 511.0f;
		camera_distance = -31.0f;
		break;
	default:
		fprintf(stderr, "unknown camera position %d\n", mode);
		exit(1);
	}
	camera_position = mode;
}

/* The view's wedge and box, from the camera's place and direction. */
static void update_view_area(float fx, float fz, int side_heading, float back)
{
	int cx = (int)(-camera_x), cz = (int)(-camera_z);
	update_visible_tiles((int)(-camera_x / 30.0f), (int)(-camera_z / 30.0f), view_octant(fx, fz));
	int h = (side_heading - 0x100) & ANGLE_MASK;
	int x2 = (int)(cos_table[h] * 30.0f * back + -camera_x);
	int z2 = (int)(sin_table[h] * 30.0f * back + -camera_z);
	float sx = cos_table[side_heading & ANGLE_MASK] * 30.0f * 10.0f;
	float sz = sin_table[side_heading & ANGLE_MASK] * 30.0f * 10.0f;
	set_view_quad((int)(-camera_x + sx), (int)(-camera_z + sz), x2, z2, (int)(-camera_x - sx),
		(int)(-camera_z - sz));
	(void)cx;
	(void)cz;
	get_road_height();
}

/* The road's slope under the car, front to back (tilt) and side to side (roll). */
static void follow_road(void)
{
	int h = (player_heading + 0x100) & ANGLE_MASK;
	float fx = cos_table[h] * 8.0f, fz = sin_table[h] * 8.0f;
	player_chunk = update_chunk_for_position(player_chunk, player_x, player_z);
	camera_lag_xz = 3.0f - fabsf(player_speed);
	if (camera_lag_xz < 1.0f)
		camera_lag_xz = 1.0f;
	player_y = ((road_height_at(player_chunk, player_x, player_z) + car_height_offset) - player_y) /
		camera_lag_xz + player_y;
	float back = road_height_at(player_chunk, player_x - fx, player_z - fz);
	float v[3] = { 2 * fx, road_height_at(player_chunk, player_x + fx, player_z + fz) - back,
		2 * fz };
	normalize_vector(v);
	camera_lag_y = 3.0f - player_speed;
	if (camera_lag_y < 1.0f)
		camera_lag_y = 1.0f;
	camera_tilt = (v[1] * 128.0f - camera_tilt) / camera_lag_y + camera_tilt;
	int s = player_heading & ANGLE_MASK;
	float sx = cos_table[s] * 4.0f, sz = sin_table[s] * 4.0f;
	back = road_height_at(player_chunk, player_x - sx, player_z - sz);
	float w[3] = { 2 * sx, road_height_at(player_chunk, player_x + sx, player_z + sz) - back,
		2 * sz };
	normalize_vector(w);
	camera_roll = (w[1] * 128.0f - camera_roll) / 3.0f + camera_roll;
}

/* update_camera_chase (and update_camera_overhead, the same from higher up). */
static void update_camera_chase(void)
{
	follow_road();
	int tilt = (int)camera_tilt, roll = (int)camera_roll;
	int h = (player_heading - look_offset + 0x100) & ANGLE_MASK;
	float fx = cos_table[h], fz = sin_table[h];
	camera_x = (-(fx * camera_distance + player_x) - camera_x) * 0.25f + camera_x;
	camera_y = (-(player_y + camera_height) - camera_y) * 0.25f + camera_y;
	camera_z = (-(fz * camera_distance + player_z) - camera_z) * 0.25f + camera_z;
	int pitch_target = atan_lookup(camera_height);
	camera_yaw = (((float)player_heading - camera_yaw) - (float)(look_offset / 2)) / 3.0f + camera_yaw;
	camera_pitch = ((float)pitch_target - camera_pitch) / 3.0f + camera_pitch;
	set_camera(0, camera_x, camera_y, camera_z, (int)camera_pitch, (int)camera_yaw, 0);
	update_view_area(fx, fz, player_heading, 10.0f);
	car_pitch = -tilt;
	car_yaw = -player_heading;
	car_roll = roll;
}

static void update_camera_cockpit(void)
{
	follow_road();
	int tilt = (int)camera_tilt, roll = (int)camera_roll;
	int h = (player_heading + 0x100) & ANGLE_MASK;
	float fx = cos_table[h], fz = sin_table[h];
	/* The original offsets z with the x component too (fx, not fz); kept. */
	camera_x = -(player_x + fx * camera_distance);
	camera_y = -(player_y + camera_height);
	camera_z = -(fx * camera_distance + player_z);
	set_camera(0, camera_x, camera_y, camera_z, tilt, player_heading, -roll);
	update_view_area(fx, fz, player_heading, 10.0f);
	car_pitch = -tilt;
	car_yaw = -player_heading;
	car_roll = roll;
}

/* place_camera_behind: behind any car (the opponent camera). */
static void place_camera_behind(float x, float y, float z, int heading)
{
	int h = (heading + 0x100) & ANGLE_MASK;
	float fx = cos_table[h], fz = sin_table[h];
	camera_x = (-(fx * camera_distance + x) - camera_x) * 0.25f + camera_x;
	camera_y = (-(y + camera_height) - camera_y) * 0.25f + camera_y;
	camera_z = (-(fz * camera_distance + z) - camera_z) * 0.25f + camera_z;
	int pitch_target = atan_lookup(camera_height);
	camera_yaw = ((float)heading - camera_yaw) / 3.0f + camera_yaw;
	camera_pitch = ((float)pitch_target - camera_pitch) / 3.0f + camera_pitch;
	set_camera(0, camera_x, camera_y, camera_z, (int)camera_pitch, (int)camera_yaw, 0);
	update_view_area(fx, fz, heading, 5.0f);
}

static void update_camera_opponent(void)
{
	const opponent_t *o = &opponent_data[watched_opponent];
	int heading = (int)((float)atan2(o->field_20, o->field_18) * 1024.0 / 6.283185307) - 0x100;
	place_camera_behind(o->x, o->y, o->z, heading);
}

void update_camera(void)
{
	switch (camera_position) {
	case CAMERA_CHASE:
	case CAMERA_OVERHEAD:
		update_camera_chase();
		break;
	case CAMERA_COCKPIT:
		update_camera_cockpit();
		objects3d[player_index].visible = 0;
		return;
	case CAMERA_OPPONENT:
		update_camera_opponent();
		break;
	default:
		fprintf(stderr, "unknown camera position %d\n", camera_position);
		exit(1);
	}
	objects3d[player_index].visible = 1;
}

void start_camera_intro(void)
{
	set_camera_position(CAMERA_CHASE);
	objects3d[player_index].visible = 1;
	camera_height = 350.0f;
	camera_distance = -31.0f;
	camera_pitch = (float)atan_lookup(350.0f);
	camera_yaw = (float)player_heading;
	camera_x = -player_x;
	camera_y = -player_y - camera_height;
	camera_z = -player_z;
}
