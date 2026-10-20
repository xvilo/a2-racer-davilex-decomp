/* The race (A2SPEL.DAT): main.cpp's WinMain, objio.cpp's race settings and states,
 * setracer.cpp's start positions, timers and ranking. */
#include "race/race.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "menu/infio.h"
#include "platform/gl.h"
#include "platform/sys.h"
#include "race/camera.h"
#include "race/driver.h"
#include "race/hud.h"
#include "race/object3d.h"
#include "race/opponent.h"
#include "race/player.h"
#include "race/race_files.h"
#include "race/race_main.h"
#include "race/render.h"
#include "race/sndfx.h"
#include "race/statobj.h"
#include "race/texture.h"
#include "race/weg.h"
#include "xvga/inifile.h"

/* The original ran a frame per screen refresh as fast as the machine allowed; the
 * port runs its frames at a fixed rate. */
#define RACE_FPS 30

track_info_t track_info[6] = {
	{ 2, "etappe1.trk", 1, 600 },
	{ 3, "etappe2.trk", 1, 900 },
	{ 4, "etappe3.trk", 1, 1200 },
	{ 5, "etappe3.trk", 0, 1200 },
	{ 6, "etappe2.trk", 0, 900 },
	{ 7, "etappe1.trk", 0, 600 },
};
int track_index;

float player_x, player_y, player_z;
float player_speed;
int player_chunk;
int player_heading;
int player_index;
int player_start_lane;

int race_state;
int race_running;
int controls_enabled;
int race_finished_ok;
int screen_resolution;
int dashboard_on_screen;
int horizon_on;
int detail_level;
int police_count = 8, civilian_count = 150;
int money, fuel, time_left;

static int texture_filtering;
static int countdown_seconds, countdown_last_tick, countdown_step;
static int intro_started, countdown_started, finish_started, finish_frame;
static int time_clock;
static int fine_count, current_fine;
static const int fine_amounts[8] = { 250, 500, 800, 1200, 1500, 2000, 2000, 2000 };

/* Racers' start and finish times (racer_finish_times: time, start, place). */
static struct {
	int finish_time;
	int start;
	int place;
} racer_times[7];
static int ranking[6];

int race_time_now(void)
{
	return (int)time(NULL);
}

/* --- timers (main.cpp, setracer.cpp) --------------------------------------------- */

static void start_countdown(int seconds)
{
	countdown_seconds = seconds;
	countdown_last_tick = race_time_now();
}

static int countdown_expired(void)
{
	if (countdown_seconds != 0) {
		int now = race_time_now();
		countdown_seconds -= now - countdown_last_tick;
		countdown_last_tick = now;
		if (countdown_seconds < 1)
			countdown_seconds = 0;
	}
	return countdown_seconds == 0;
}

static void init_race_timers(int limit)
{
	time_left = limit;
	time_clock = race_time_now();
	for (int i = 0; i < 7; i++) {
		racer_times[i].start = race_time_now();
		racer_times[i].finish_time = 0;
		racer_times[i].place = 1;
	}
}

static void restart_time_clock(void)
{
	time_clock = race_time_now();
}

static void update_time_left(void)
{
	if (time_left == 0)
		return;
	int now = race_time_now();
	time_left -= now - time_clock;
	time_clock = now;
	if (time_left < 1) {
		infio.ended_by = 3;
		race_finished_ok = 0;
		set_race_state(RACE_OUT_OF_TIME);
		time_left = 0;
		play_sound_effect(0x26, 0);
	}
}

void consume_fuel(int amount)
{
	fuel -= amount;
	if (fuel < 1) {
		infio.ended_by = 1;
		race_finished_ok = 0;
		set_race_state(RACE_OUT_OF_FUEL);
		play_sound_effect(0x27, 0);
		fuel = 0;
	}
}

void pay_money(int amount)
{
	money -= amount;
	if (money < 0) {
		infio.ended_by = 2;
		race_finished_ok = 0;
		set_race_state(RACE_NO_MONEY);
		play_sound_effect(0x25, 0);
		money = 0;
	}
}

static void next_fine(void)
{
	current_fine = fine_amounts[fine_count++];
	if (5 < fine_count)
		fine_count = 5;
}

/* --- ranking (setracer.cpp) ------------------------------------------------------ */

/* racer_progress_in_section: how far along its chunk a racer is. */
static float racer_progress_in_section(int racer)
{
	const opponent_t *o = &opponent_data[racer];
	const weg_section_t *s = &weg_sections[o->chunk];
	const vertex3d_t *p = &vertices3d[s->point];
	int h = s->heading & ANGLE_MASK;
	return (o->x - p->x) * cos_table[h] + (o->z - p->z) * sin_table[h];
}

const int *compute_race_ranking(void)
{
	int progress[6], key[6], sorted[6];
	if (!opponent_data || opponent_count < 6)
		return NULL;
	for (int i = 0; i < 6; i++) {
		progress[i] = (int)racer_progress_in_section(i);
		key[i] = sorted[i] = opponent_data[i].chunk * 100 + progress[i];
	}
	/* bubble_sort_ints */
	for (int swapped = 1; swapped;) {
		swapped = 0;
		for (int j = 0; j < 5; j++) {
			if (sorted[j + 1] < sorted[j]) {
				int t = sorted[j];
				sorted[j] = sorted[j + 1];
				sorted[j + 1] = t;
				swapped++;
			}
		}
	}
	for (int i = 0; i < 6; i++) {
		for (int k = 0; k < 6; k++) {
			if (key[i] == sorted[k]) {
				ranking[i] = track_info[track_index].direction == 0 ? k + 1 : 6 - k;
				break;
			}
		}
	}
	return ranking;
}

int get_racer_entry(int racer)
{
	const int *r = compute_race_ranking();
	return r ? r[racer] : racer + 1;
}

void set_racer_finish_time(int racer)
{
	if (racer_times[racer].finish_time == 0) {
		racer_times[racer].finish_time = race_time_now() - racer_times[racer].start;
		racer_times[racer].place = get_racer_entry(racer);
	}
}

/* A racer still on the road gets the player's time and up to a minute more. */
static int get_racer_finish_time(int racer)
{
	if (racer_times[racer].finish_time != 0)
		return racer_times[racer].finish_time;
	return racer_times[0].finish_time + rand() % 0x3c + 5;
}

static int get_racer_place(int racer)
{
	if (racer_times[racer].finish_time != 0)
		return racer_times[racer].place;
	return get_racer_entry(racer);
}

/* --- start (setracer.cpp) -------------------------------------------------------- */

/* The grid: two cars a row behind the start, alternately in the lanes left and right. */
static void set_racer_start_position(int racer, int position, int start_chunk)
{
	int direction = track_info[track_index].direction;
	int lane = direction == 0 ? 8 : 2, sign = direction == 0 ? -1 : 1;
	float offset = direction == 0 ? -20.0f : 20.0f;
	float side = position % 2 == 1 ? 1.0f : -1.0f;
	int chunk = start_chunk - ((position - 1) / 2 + 1) * sign;
	lane += (position % 2) * sign;
	if (racer == 0) {
		load_player_object(direction, chunk, lane, side * offset);
	}
	else {
		place_opponent(racer, direction, chunk, lane, side * offset);
		driver_list[opponent_data[racer].field_e0].speed = (float)racer_speed;
	}
}

/* The racers 1..5, police and traffic (create_all_opponents): opponent numbers 1..5 are
 * the rivals, then the police, then the civilians in turn of car type. */
static void create_all_opponents(void)
{
	create_opponent(0x15, DRIVER_RIVAL);
	create_opponent(0x12, DRIVER_RIVAL);
	create_opponent(0x11, DRIVER_RIVAL);
	create_opponent(0x16, DRIVER_RIVAL);
	create_opponent(0x14, DRIVER_RIVAL);
	for (int i = 0; i < police_count; i++)
		create_opponent(0xc, DRIVER_POLICE);
	for (int j = 0, type = 0; j < civilian_count; j++) {
		create_opponent(type, DRIVER_CIVILIAN);
		do {
			if (++type == 0x10)
				type = 0;
		} while (type == 4 || type == 0xc);
	}
}

static void place_rival_racers(int start_chunk)
{
	set_racer_start_position(1, infio.racer_1, start_chunk);
	set_racer_start_position(2, infio.racer_2, start_chunk);
	set_racer_start_position(3, infio.racer_3, start_chunk);
	set_racer_start_position(4, infio.racer_4, start_chunk);
	set_racer_start_position(5, infio.racer_5, start_chunk);
}

/* A police car parked on the hard shoulder (the two outer lanes). */
static void set_police_car(int police, int chunk)
{
	int direction = track_info[track_index].direction;
	int lane = direction == 0 ? 10 - rand() % 2 : rand() % 2;
	place_opponent(police + 6, direction, chunk, lane, 0.0f);
}

/* Spread along the road, one per stretch of (chunks / police / 2) or so. */
static void place_police_cars(void)
{
	int stretch = weg_chunk_count / (police_count * 2), chunk = 0;
	for (int police = 0; police < police_count; police++) {
		chunk += rand() % stretch + stretch;
		set_police_car(police, chunk);
	}
}

static int player_at_finish(void)
{
	if (track_info[track_index].direction == 0)
		return player_chunk == 0xf;
	return weg_chunk_count - 0xf == player_chunk;
}

static void set_all_objects(void)
{
	int position = infio.position;
	if (track_info[track_index].direction == 0) {
		set_racer_start_position(0, position, weg_chunk_count - 0xf);
		create_all_opponents();
		place_rival_racers(weg_chunk_count - 0xf);
		place_police_cars();
		scatter_traffic(police_count + 6, 0x1e);
		/* TODO: place_random_pionnen */
		place_static_object_on_road(0x17, 0xf, 1, 1, 0x3c);
		place_static_object_on_road(0x16, weg_chunk_count - 0x10, 1, 1, 0x3c);
	}
	else {
		set_racer_start_position(0, position, 0xf);
		create_all_opponents();
		place_rival_racers(0xf);
		place_police_cars();
		scatter_traffic(police_count + 6, 0x1e);
		/* TODO: place_random_pionnen */
		place_static_object_on_road(0x16, 0x10, 2, 1, 0x3c);
		place_static_object_on_road(0x17, weg_chunk_count - 0xf, 2, 1, 0x3c);
	}
	for (int type = 0x26; type <= 0x2b; type++)
		load_static_object(type, 0.0f, 0.0f, 0);
}

/* --- settings and states (objio.cpp) --------------------------------------------- */

static int race_positions_valid(void)
{
	const int16_t p[6] = { infio.position, infio.racer_1, infio.racer_2, infio.racer_3,
		infio.racer_4, infio.racer_5 };
	for (int i = 0; i < 6; i++) {
		if (p[i] < 1 || p[i] > 6)
			return 0;
		for (int j = 0; j < 6; j++) {
			if (i != j && p[i] == p[j])
				return 0;
		}
	}
	return 1;
}

static void init_race_settings(void)
{
	init_infio();
	if (!race_disc_ini("A2.INI")) {
		/* TRACK/START1..3: the time limits of stages 1..3, and 4..6 driving them back. */
		int start[3] = {
			(int)read_ini_value_setting("TRACK", "START1") * 0x3c,
			(int)read_ini_value_setting("TRACK", "START2") * 0x3c,
			(int)read_ini_value_setting("TRACK", "START3") * 0x3c,
		};
		track_info[0].time_limit = track_info[5].time_limit = start[0];
		track_info[1].time_limit = track_info[4].time_limit = start[1];
		track_info[2].time_limit = track_info[3].time_limit = start[2];
	}
	if (!race_positions_valid()) {
		infio.racer_1 = 1;
		infio.racer_2 = 2;
		infio.racer_3 = 3;
		infio.racer_4 = 4;
		infio.racer_5 = 5;
		infio.position = 6;
	}
	fuel = 300;
	race_running = 0;
	controls_enabled = 0;
	detail_level = infio.detail;
	race_finished_ok = 0;
	track_index = infio.stage;
	if (track_index < 0 || 5 < track_index)
		track_index = 0;
	init_race_timers(track_info[track_index].time_limit);
	money = infio.money;
	if (infio.money < 1 || 9999 < infio.money)
		money = 1000;
	dashboard_on_screen = infio.dashboard == 1;
	/* Display modes: 0 3D card (bilinear textures), 1 320x200, 2 640x480. The port always
	 * draws at the window's size; 320x200's smaller HUD is kept. */
	texture_filtering = infio.display_mode == 0;
	screen_resolution = infio.display_mode == 1;
	if (infio.display_mode == 1)
		dashboard_on_screen = 0;
	horizon_on = infio.horizon == 1;
}

int set_race_state(int state)
{
	if ((race_state == RACE_RUNNING || state == RACE_RUNNING || state == RACE_EXIT) &&
		race_state != RACE_EXIT)
		race_state = state;
	switch (race_state) {
	case RACE_RUNNING:
		race_running = 1;
		controls_enabled = 1;
		return 0;
	case RACE_FINE:
		next_fine();
		start_countdown(7);
		play_sound_effect(0x23, 0);
		return 0x23;
	case RACE_OUT_OF_TIME:
	case RACE_OUT_OF_FUEL:
		halt_player_car();
		start_countdown(7);
		controls_enabled = 0;
		return 0;
	case RACE_NO_MONEY:
		start_countdown(0xc);
		controls_enabled = 0;
		return 0;
	case RACE_PAUSED:
		race_running = 0;
		controls_enabled = 0;
		return 9;
	default:
		return race_state;
	}
}

/* update_race: the states' timing and the pictures they show. */
static void update_race(void)
{
	int dash = dashboard_on_screen;
	switch (race_state) {
	case RACE_INTRO:
		if (!intro_started) {
			start_camera_intro();
			intro_started = 1;
		}
		camera_height -= 10.0f;
		if (camera_height <= 0.0f) {
			set_camera_position(CAMERA_COCKPIT);
			race_state = RACE_COUNTDOWN;
		}
		break;
	case RACE_COUNTDOWN:
		if (!countdown_started) {
			countdown_step = 0;
			start_countdown(1);
			countdown_started = 1;
		}
		if (countdown_expired()) {
			countdown_step++;
			if (countdown_step == 5) {
				race_running = 1;
				controls_enabled = 1;
				race_state = RACE_RUNNING;
				countdown_step = 4;
			}
			else {
				play_sound_effect(countdown_step < 4 ? 0x1a : 0x1b, 0);
				start_countdown(1);
			}
		}
		hud_image(0x12, countdown_step, 0x96, dash ? 0xa0 : 200, 2);
		break;
	case RACE_FINISHED:
		if (!finish_started) {
			stop_player_car();
			start_countdown(10);
			finish_started = 1;
		}
		if (countdown_expired() || keyispressed(0x20) || keyispressed(0x0d))
			race_state = RACE_EXIT;
		/* The waving flag. */
		hud_image(0x13, finish_frame, 0x140, dash ? 0xa0 : 200, 0);
		finish_frame = (finish_frame + 1) % 0x10;
		break;
	case RACE_FINE: {
		int x = 0x186, y = dash ? 0xbe : 0xe6;
		hud_image(0x16, 0, 0xa0, dash ? 0x3c : 100, 2);
		if (current_fine / 1000 != 0)
			hud_image(0x1c, current_fine / 1000, x, y, 0);
		hud_image(0x1c, current_fine % 1000 / 100, x + 0xc, y, 0);
		hud_image(0x1c, current_fine % 100 / 10, x + 0xc * 2, y, 0);
		hud_image(0x1c, current_fine % 10, x + 0xc * 3, y, 0);
		if (countdown_expired() || keyispressed(0x0d) || keyispressed(0x20)) {
			set_race_state(RACE_RUNNING);
			pay_money(current_fine);
		}
		break;
	}
	case RACE_OUT_OF_TIME:
	case RACE_OUT_OF_FUEL:
		hud_image(race_state == RACE_OUT_OF_TIME ? 0x20 : 0x22, 0, 0x140, dash ? 0x8c : 0xf0, 0);
		if (countdown_expired())
			set_race_state(RACE_EXIT);
		break;
	case RACE_NO_MONEY:
		hud_image(0x18, 0, 0xa0, dash ? 0x3c : 100, 2);
		if (countdown_expired() || keyispressed(0x0d) || keyispressed(0x20))
			set_race_state(RACE_EXIT);
		break;
	case RACE_PAUSED:
		hud_image(0x1a, 0, 0xa0, dash ? 100 : 0x8c, 2);
		if (keyispressed(0x50) || keyispressed(0x0d) || keyispressed(0x20)) {
			while (keyispressed(0x50)) {
				sys_input in;
				sys_poll(&in);
				sys_sleep(5);
			}
			restart_time_clock();
			set_race_state(RACE_RUNNING);
		}
		break;
	}
}

/* --- WinMain --------------------------------------------------------------------- */

/* WinMain's A2.INI settings. It reads POLICE<n> and CIVIL<n> before the stage is known,
 * so stage 1's numbers count for every stage; kept. */
static void read_tweaks(void)
{
	police_count = 8;
	civilian_count = 0x96;
	if (race_disc_ini("A2.INI"))
		return;
	racer_range = (int)read_ini_value_setting("TWEAK", "RACERRANGE");
	racer_high_speed = (float)read_ini_value_setting("TWEAK", "RACERHIGHSPEED");
	racer_low_speed = (float)read_ini_value_setting("TWEAK", "RACERLOWSPEED");
	police_count = (int)read_ini_value_setting("TWEAK", "POLICE0");
	civilian_count = (int)read_ini_value_setting("TWEAK", "CIVIL0");
	police_max_speed = (int)read_ini_value_setting("RACE", "POLICESPEED");
	racer_speed = (int)read_ini_value_setting("RACE", "RACERSPEED");
	if (police_count == 0)
		police_count = 8;
	if (civilian_count == 0)
		civilian_count = 0x96;
}

static int run(game_t *g)
{
	read_tweaks();
	init_race_settings();
	init_math3d();
	alloc_textures(0x50);
	change_viewport_size(0, 0, 640, dashboard_on_screen ? 300 : 480);
	set_background("back.pcx");
	if (init_hud(g))
		return 1;
	if (texture_filtering)
		set_texture_filter(1);
	/* init_anims_and_standard_textures */
	int objects = 0x9c + police_count + civilian_count + 400;
	if (alloc_objects3d(objects) || alloc_3d_arrays(objects, objects, 200000, 100000)) {
		fprintf(stderr, "memory error initing anims\n");
		return 1;
	}
	load_car_textures();
	load_static_object_textures();
	road_texture = load_texture("2baans.pcx", 0);
	grass_texture = load_texture("grass2.pcx", 0);
	if (init_opponents(police_count + 0x38 + civilian_count))
		return 1;
	if (load_weg_data(track_info[track_index].track, 0x14))
		return 1;
	init_opponent_map();
	init_drivers(opponent_count, 5, police_count);
	init_active_opponent_list(opponent_count);
	init_cameras();
	init_controls();
	if (horizon_on)
		load_static_object(0, 0.0f, 0.0f, 0);
	set_all_objects();
	if (detail_level == 1)
		init_static_objects(track_index);
	race_state = RACE_INTRO;
	intro_started = countdown_started = finish_started = 0;
	start_camera_intro();

	uint32_t next = sys_ticks();
	sys_input in = { 0 };
	while (race_state != RACE_EXIT) {
		sys_poll(&in);
		if (in.quit) {
			infio.ended_by = 0;
			race_finished_ok = 0;
			break;
		}
		update_racers();
		sys_gl_begin_frame();
		render_3d_frame();
		hud_begin();
		draw_radar();
		/* draw_time_left keeps the clock still until the race runs. */
		if (!race_running)
			restart_time_clock();
		update_time_left();
		draw_dashboard();
		update_race();
		hud_end();
		sys_gl_swap();
		mark_active_racers();
		if (keyispressed(0x50) && race_state == RACE_RUNNING) {
			while (keyispressed(0x50)) {
				sys_poll(&in);
				sys_sleep(5);
			}
			set_race_state(RACE_PAUSED);
		}
		if (player_at_finish() && race_state != RACE_EXIT && race_state != RACE_FINISHED) {
			set_racer_finish_time(0);
			play_sound_effect(0x2b, 0);
			race_state = RACE_FINISHED;
			race_finished_ok = 1;
		}
		if (keyispressed(0x70))
			set_camera_position(CAMERA_COCKPIT);
		if (keyispressed(0x71))
			set_camera_position(CAMERA_CHASE);
		if (keyispressed(0x74))
			radar_on = !radar_on;
		if (keyispressed(0x1b) && race_state == RACE_RUNNING) {
			infio.ended_by = 0;
			race_finished_ok = 0;
			race_state = RACE_EXIT;
		}
		if (keyispressed(0x5a) && keyispressed(0x4f) && keyispressed(0x11) && keyispressed(0x7b))
			set_camera_position(CAMERA_OVERHEAD);
		/* A fixed frame rate. */
		next += 1000 / RACE_FPS;
		int32_t wait = (int32_t)(next - sys_ticks());
		if (wait > 0)
			sys_sleep((uint32_t)wait);
		else if (wait < -200)
			next = sys_ticks();
	}

	/* The result for the menu. */
	if (!race_finished_ok) {
		infio.time = 0;
	}
	else {
		infio.time = get_racer_finish_time(0);
		infio.money = money;
		infio.position = (int16_t)get_racer_place(0);
		infio.racer_1 = (int16_t)get_racer_place(1);
		infio.racer_2 = (int16_t)get_racer_place(2);
		infio.racer_3 = (int16_t)get_racer_place(3);
		infio.racer_4 = (int16_t)get_racer_place(4);
		infio.racer_5 = (int16_t)get_racer_place(5);
		infio.time_racer_1 = get_racer_finish_time(1);
		infio.time_racer_2 = get_racer_finish_time(2);
		infio.time_racer_3 = get_racer_finish_time(3);
		infio.time_racer_4 = get_racer_finish_time(4);
		infio.time_racer_5 = get_racer_finish_time(5);
	}
	infio.race_finished = (uint8_t)(race_finished_ok != 0);
	save_infio();
	exit_hud();
	(void)g;
	return 0;
}

static void free_race(void)
{
	exit_drivers();
	exit_opponent_map();
}

int race_main(game_t *g)
{
	if (!sys_gl()) {
		fprintf(stderr, "the race needs OpenGL\n");
		return 1;
	}
	if (race_files_open(g->data_dir))
		return 1;
	int r = run(g);
	free_race();
	exit_opponents();
	free_weg_data();
	free_objects3d();
	free_3d_arrays();
	free_textures();
	race_files_close();
	return r;
}
