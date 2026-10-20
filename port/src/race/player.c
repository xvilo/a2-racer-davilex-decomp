#include "race/player.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>


#include "menu/infio.h"
#include "platform/sys.h"
#include "race/camera.h"
#include "race/object3d.h"
#include "race/opponent.h"
#include "race/race.h"
#include "race/render.h"
#include "race/sndfx.h"
#include "race/texture.h"
#include "race/weg.h"

car_type_t car_types[CAR_TYPE_COUNT] = {
	{ "porche.d3d", "porche.pcx", "k_porche.pcx", -1, -1, 9.0f, 0, 900.0f },
	{ "interl.d3d", "il.pcx", "k_il.pcx", -1, -1, 7.0f, 0, 5000.0f },
	{ "camaro.d3d", "camaro.pcx", "k_camaro.pcx", -1, -1, 9.0f, 0, 1500.0f },
	{ "golf.d3d", "golf.pcx", "k_golf.pcx", -1, -1, 8.5f, 0, 1000.0f },
	{ "bmw.d3d", "bmw.pcx", "k_bmw.pcx", -1, -1, 10.0f, 0, 1300.0f },
	{ "van.d3d", "vanmap.pcx", "k_vanmap.pcx", -1, -1, 9.0f, 0, 1000.0f },
	{ "lotus.d3d", "lotus.pcx", "k_lotus.pcx", -1, -1, 11.0f, 0, 1500.0f },
	{ "viper.d3d", "viper.pcx", "k_viper.pcx", -1, -1, 10.0f, 0, 1500.0f },
	{ "panda.d3d", "panda.pcx", "k_panda.pcx", -1, -1, 6.0f, 0, 800.0f },
	{ "merceds.d3d", "mercedes.pcx", "k_merced.pcx", -1, -1, 8.0f, 0, 900.0f },
	{ "merceds.d3d", "merceds2.pcx", "k_merce2.pcx", -1, -1, 8.5f, 0, 900.0f },
	{ "vracht.d3d", "vracht.pcx", "k_vracht.pcx", -1, -1, 8.0f, 0, 10000.0f },
	{ "police.d3d", "police.pcx", "k_police.pcx", -1, -1, 18.0f, 0, 900.0f },
	{ "cx.d3d", "cx.pcx", "k_cx.pcx", -1, -1, 10.0f, 0, 900.0f },
	{ "jaguar.d3d", "jaguar.pcx", "k_jaguar.pcx", -1, -1, 10.0f, 0, 900.0f },
	{ "volvo.d3d", "volvo.pcx", "k_volvo.pcx", -1, -1, 10.0f, 0, 900.0f },
	{ "werkweg2.d3d", "werkweg2.pcx", "werkweg2.pcx", -1, -1, 0.0f, 0, 10000.0f },
	{ "porche.d3d", "r_porche.pcx", "r_porche.pcx", -1, -1, 9.0f, 0, 900.0f },
	{ "camaro.d3d", "r_camaro.pcx", "r_camaro.pcx", -1, -1, 9.0f, 0, 1500.0f },
	{ "bmw.d3d", "r_bmw.pcx", "r_bmw.pcx", -1, -1, 10.0f, 0, 1300.0f },
	{ "jaguar.d3d", "r_jaguar.pcx", "r_jaguar.pcx", -1, -1, 10.0f, 0, 900.0f },
	{ "lotus.d3d", "r_lotus.pcx", "r_lotus.pcx", -1, -1, 11.0f, 0, 1500.0f },
	{ "viper.d3d", "r_viper.pcx", "r_viper.pcx", -1, -1, 10.0f, 0, 1500.0f },
};

/* lane_offsets: the lanes' distances from the centre (positive: to the right). */
static const float lane_offsets[11] = {
	108.0f, 85.0f, 60.0f, 30.0f, 0.0f, 0.0f, 0.0f, -30.0f, -60.0f, -85.0f, -108.0f,
};

/* Heading change per frame by steering position (0 full left .. 28 full right), and the
 * divisor by speed: faster cars turn less. */
static const float steer_table[29] = {
	14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, -1, -2, -3, -4, -5, -6, -7, -8, -9, -10,
	-11, -12, -13, -14,
};
static const float steer_speed_divisor[16] = {
	1.01f, 1.1f, 1.2f, 1.3f, 1.3f, 1.3f, 1.3f, 1.3f, 1.4f, 1.5f, 1.5f, 1.6f, 1.8f, 2.0f, 2.0f, 2.0f,
};

/* The gearbox: steps of (rpm, speed, gear), filled by add_rpm_steps. */
typedef struct {
	int rpm;
	float speed;
	int gear;
} rpm_step_t;

static rpm_step_t rpm_table[200];
static int rpm_step_count, rpm_step, idle_rpm_step, current_gear;
int engine_rpm;

static int key_left, key_right, key_gas, key_brake;
static int control_type;          /* 1 keyboard, 2 joypad, 3 steering wheel */
static int steering_position = 14; /* 0..28, 14 straight */
static float steering_return_rate = 1.0f;
static int steer_index = 14;      /* DAT_00457750: the position the car steers with */
static int accelerator_input;     /* 0 none yet, 1 forward, 2 reverse */
static int wrong_way_warned;
static int engine_started;

void load_car_textures(void)
{
	for (int i = 0; i < CAR_TYPE_COUNT; i++)
		car_types[i].texture_id = load_texture(car_types[i].texture, 0);
}

/* add_rpm_steps: `count` steps for a gear, speed and rpm rising evenly. */
static void add_rpm_steps(int gear, float speed_from, float speed_to, float rpm_from,
	float rpm_to, float count)
{
	float dspeed = (speed_to - speed_from) / count, drpm = (rpm_to - rpm_from) / count;
	float speed = speed_from, rpm = rpm_from;
	for (float k = 0.0f; k < count; k += 1.0f) {
		rpm_step_t *s = &rpm_table[rpm_step_count++];
		s->rpm = (int)rpm;
		s->speed = speed;
		s->gear = gear;
		rpm += drpm;
		speed += dspeed;
	}
}

void init_controls(void)
{
	key_left = infio.key_left;
	key_right = infio.key_right;
	key_gas = infio.key_gas;
	key_brake = infio.key_brake;
	control_type = infio.control_type == 1 ? 2 : infio.control_type == 2 ? 3 : 1;
	rpm_step_count = 0;
	add_rpm_steps(0, -2.5f, 0.0f, 50.0f, 0.0f, 7.0f); /* reverse */
	idle_rpm_step = rpm_step = rpm_step_count;
	add_rpm_steps(1, 0.0f, 2.5f, 0.0f, 55.0f, 7.0f);
	add_rpm_steps(2, 2.5f, 4.0f, 35.0f, 60.0f, 12.0f);
	add_rpm_steps(3, 4.0f, 6.0f, 40.0f, 63.0f, 15.0f);
	add_rpm_steps(4, 6.0f, 8.0f, 45.0f, 65.0f, 30.0f);
	add_rpm_steps(5, 8.0f, 15.0f, 45.0f, 60.0f, 80.0f);
	steering_position = steer_index = 14;
	accelerator_input = 0;
	wrong_way_warned = engine_started = 0;
}

void select_rpm_step_for_speed(float speed)
{
	rpm_step = 0;
	for (int i = 0; i < rpm_step_count; i++) {
		if (rpm_table[i].speed <= speed)
			rpm_step = i;
	}
}

/* update_steering: 1 left, 2 right, 3 neither (back to straight). */
static void update_steering(int input)
{
	if (input == 1) {
		if (steering_position > 0) {
			steering_position = (int)(steering_position - steering_return_rate);
			if (steering_position < 0)
				steering_position = 0;
		}
	}
	else if (input == 2) {
		if (steering_position < 28) {
			steering_position = (int)(steering_position + steering_return_rate);
			if (steering_position > 28)
				steering_position = 27;
		}
	}
	if (input == 1 || input == 2) {
		steering_return_rate += 0.5f;
		if (steering_return_rate > 6.0f)
			steering_return_rate = 6.0f;
	}
	else if (input == 3) {
		if (steering_position > 14) {
			steering_position = (int)(steering_position - steering_return_rate);
			if (steering_position < 14)
				steering_position = 14;
		}
		if (steering_position < 14) {
			steering_position = (int)(steering_position + steering_return_rate);
			if (steering_position > 14)
				steering_position = 14;
		}
		steering_return_rate = 1.0f;
	}
	if (look_offset == 0)
		steer_index = steering_position;
}

/* update_accelerator: 1 more throttle (in the current direction), 2 brake, 3 neither. */
static void update_accelerator(int input)
{
	int boost_freq = 0, boost_vol = 0;
	if (input == 1 && accelerator_input != 0) {
		if (accelerator_input == 1) {
			if (rpm_step < rpm_step_count - 1)
				rpm_step++;
		}
		else if (rpm_step > 0) {
			rpm_step--;
		}
		boost_freq = 6000;
		boost_vol = 300;
	}
	else if ((input == 2 || input == 3) && accelerator_input != 0) {
		int step = input == 2 ? 4 : 1; /* braking or rolling out */
		if (accelerator_input == 1) {
			if (rpm_step > idle_rpm_step) {
				rpm_step -= step;
				if (rpm_step < idle_rpm_step)
					rpm_step = idle_rpm_step;
			}
		}
		else if (rpm_step < idle_rpm_step) {
			rpm_step += step;
			if (rpm_step > idle_rpm_step)
				rpm_step = idle_rpm_step;
		}
	}
	if (current_gear != rpm_table[rpm_step].gear)
		play_gear_change_sound(rpm_table[rpm_step].gear, current_gear);
	engine_rpm = rpm_table[rpm_step].rpm;
	player_speed = rpm_table[rpm_step].speed;
	current_gear = rpm_table[rpm_step].gear;
	if (race_running)
		set_engine_sound(engine_rpm, boost_freq, boost_vol);
}

/* A port-only test aid (A2_AUTOPILOT=1): keeps the car in the lane it starts in,
 * below police-alerting speed, so scripts can drive a whole stage. */
static int autopilot = -1;
static float autopilot_lane;

static int autopilot_steer(void)
{
	if (autopilot < 0)
		autopilot = getenv("A2_AUTOPILOT") != NULL;
	if (!autopilot)
		return 0;
	const weg_chunk_t *ch = &weg_chunks[player_chunk];
	const vertex3d_t *c = &vertices3d[weg_sections[ch->section].point];
	float lateral = (player_x - c->x) * ch->side[0] + (player_z - c->z) * ch->side[2];
	if (autopilot_lane == 0.0f)
		autopilot_lane = lateral;
	/* Aim 160 ahead on the road at the lane's offset. */
	int ahead = player_chunk + (track_info[track_index].direction == 1 ? 2 : -2);
	if (ahead < 0)
		ahead = 0;
	if (ahead >= weg_chunk_count)
		ahead = weg_chunk_count - 1;
	const weg_chunk_t *ah = &weg_chunks[ahead];
	const vertex3d_t *a = &vertices3d[weg_sections[ah->section].point];
	float tx = a->x + ah->side[0] * autopilot_lane - player_x;
	float tz = a->z + ah->side[2] * autopilot_lane - player_z;
	int h = (player_heading + 0x100) & ANGLE_MASK;
	float cross = cos_table[h] * tz - sin_table[h] * tx;
	return cross > 2.0f ? -1 : cross < -2.0f ? 1 : 0;
}

void handle_player_controls(void)
{
	if (!engine_started && race_running) {
		start_engine_sounds();
		engine_started = 1;
	}
	if (!controls_enabled)
		return;
	/* TODO: joypad and steering wheel (control types 2 and 3). */
	int steer = autopilot_steer();
	if (keyispressed(key_left) || steer < 0)
		update_steering(1);
	else if (keyispressed(key_right) || steer > 0)
		update_steering(2);
	else
		update_steering(3);

	opponent_t *op = &opponent_data[objects3d[player_index].opponent];
	op->speed = player_speed;
	if (fabsf(player_speed) >= 0.01f && accelerator_input != 0) {
		int s = abs((int)player_speed);
		float turn = steer_table[steer_index] / steer_speed_divisor[s];
		player_heading = (int)((float)player_heading + (accelerator_input == 1 ? turn : -turn));
		op->speed = player_speed;
	}

	int input;
	if (keyispressed(key_gas) || (autopilot && player_speed < 9.0f)) {
		input = 1;
		accelerator_input = 1;
	}
	else if (keyispressed(key_brake)) {
		if (rpm_step == idle_rpm_step)
			accelerator_input = 2;
		input = accelerator_input == 1 ? 2 : accelerator_input == 2 ? 1 : 0;
	}
	else {
		input = 3;
	}
	if (input)
		update_accelerator(input);

	/* Every new chunk costs fuel; going backwards gets a warning once. */
	if (player_start_lane == player_chunk)
		return;
	consume_fuel(1);
	int forward = track_info[track_index].direction == 1 ? player_start_lane <= player_chunk
		: player_chunk <= player_start_lane;
	if (!forward && !wrong_way_warned) {
		wrong_way_warned = 1;
		play_sound_effect(0x2f, 0);
	}
	player_start_lane = player_chunk;
}

void halt_player_car(void)
{
	player_speed = 0.0f;
	opponent_data[objects3d[player_index].opponent].speed = 0.0f;
}

static int update_player_object(int object)
{
	const opponent_t *op = &opponent_data[objects3d[object].opponent];
	matrix_t m;
	matrix_identity(&m);
	m.m[12] = op->x;
	m.m[13] = op->y;
	m.m[14] = op->z;
	rotate_matrix_y(&m, car_yaw + look_offset);
	rotate_matrix_x(&m, car_pitch);
	rotate_matrix_z(&m, car_roll);
	objects3d[object].matrix = m;
	return 0;
}

void load_player_object(int direction, int chunk, int lane, float offset)
{
	player_index = load_object_file(car_types[PLAYER_CAR_TYPE].mesh);
	object3d_t *o = &objects3d[player_index];
	o->texture = car_types[PLAYER_CAR_TYPE].texture_id;
	matrix_t m;
	matrix_identity(&m);
	m.m[14] = 1.0f; /* D3DMATRIX_004576d0 */
	set_object3d_matrix(player_index, &m);
	o->update = update_player_object;
	build_object_execute_buffer(player_index);
	o->field_36 = 1;
	int op = alloc_opponent_slot();
	o->opponent = op;
	opponent_t *p = &opponent_data[op];
	p->object = player_index;
	p->mass = 1000.0f;
	p->active = 1;
	p->push_x = p->push_z = 0.0f;
	p->offset_x = p->offset_z = 0.0f;
	o->update_interval = 1;
	p->waypoint = chunk;
	p->chunk = chunk;
	p->direction = direction;
	p->field_e0 = -1;
	p->field_e4 = 1;
	p->lane = p->target_lane = lane;
	const weg_chunk_t *ch = &weg_chunks[chunk];
	const vertex3d_t *c = &vertices3d[weg_sections[ch->section].point];
	float l = lane_offsets[lane];
	player_y = 0.0f;
	/* side * lane + forward * (offset + 40); forward = (-side.z, side.x). */
	player_x = (ch->side[0] * l + c->x) - ch->side[2] * (offset + 40.0f);
	player_z = (offset + 40.0f) * ch->side[0] + ch->side[2] * l + c->z;
	p->x = player_x;
	p->y = player_y;
	p->z = player_z;
	player_heading = weg_sections[chunk].heading + (direction == 0 ? 0x100 : -0x100);
	p->heading = player_heading;
	player_start_lane = chunk;
	player_chunk = chunk;
	compute_object_bounds(player_index);
}

void stop_player_car(void)
{
	halt_player_car();
	controls_enabled = 0;
}
