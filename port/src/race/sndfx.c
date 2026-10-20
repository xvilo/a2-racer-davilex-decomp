#include "race/sndfx.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "race/object3d.h"
#include "race/opponent.h"
#include "race/race.h"
#include "race/race_files.h"
#include "xvga/sound.h"

int engine_sound_enabled, music_enabled;
int volume_attenuation, volume_offset, music_attenuation;

/* The sample table at 0x458db4 (15 bytes a name). */
static const char *const sample_names[0x30] = {
	"motor2.wav", "motor3.wav", "speed.wav", "energy.wav", "jammin.wav", "moody.wav",
	"disco.wav", "rock.wav", "sirene.wav", "bump.wav", "bump2.wav", "bump3.wav",
	"bump4.wav", "start.wav", "mstart.wav", "truck01.wav", "truck02.wav", "truck03.wav",
	"truck05.wav", "truck06.wav", "toet01.wav", "toet03.wav", "toet05.wav", "toet06.wav",
	"autos1.wav", "autos2.wav", "set.wav", "go.wav", "sp_b1.wav", "sp_b2.wav",
	"sp_b3.wav", "sp_p1.wav", "sp_p2.wav", "sp_p3.wav", "sp_p4.wav", "sp_p5.wav",
	"sp_p6.wav", "sp_p7.wav", "sp_p8.wav", "sp_p9.wav", "bonusb.wav", "bonusg.wav",
	"bonust.wav", "finish.wav", "loser.wav", "sp_r1.wav", "sp_r2.wav", "sp_r3.wav",
};

/* Volume by distance (|dx| + |dy| + |dz|, 0..499). */
static int volume_by_distance[500];

/* Six engine sounds of the cars nearest the player: buffer and object. */
static struct {
	int handle;
	int object; /* -1 for a free slot */
} object_sounds[6];

static int music_handle = -1;

void load_samples(void)
{
	/* The race's own process opened the sound with RCS in the original. */
	xvga_set_sound_files(race_archive(), 0x3c);
	for (int i = 0; i < 500; i++)
		volume_by_distance[i] = 0xdac - 7 * i - 4000;
	for (int i = 0; i < 6; i++) {
		object_sounds[i].handle = -1;
		object_sounds[i].object = -1;
	}
}

void exit_samples(void)
{
	stop_all_sound_buffers();
	music_handle = -1;
}

void start_music(int music)
{
	if (music_enabled != 0) {
		music_handle = play_sound_buffer_1(sample_names[music], SOUND_LOOPING);
		set_sound_buffer_vol(music_handle, -100 - music_attenuation);
	}
}

void play_sound_effect(int effect, int loop)
{
	if (engine_sound_enabled == 0)
		return;
	if (effect < 0 || 0x2f < effect) {
		fprintf(stderr, "SNDFX sample out of bounds\n");
		exit(1);
	}
	int handle = play_sound_buffer_2(sample_names[effect], -100 - volume_attenuation, 0);
	set_sound_buffer_vol(handle, -100 - volume_attenuation);
	while (loop != 0)
		loop = sound_buffer_busy(handle);
}

static int distance_to_player(float x, float y, float z)
{
	return (int)(fabsf(x - player_x) + fabsf(y - player_y) + fabsf(z - player_z));
}

void play_crash_sound(float x, float y, float z, float speed)
{
	if (engine_sound_enabled == 0)
		return;
	int d = distance_to_player(x, y, z);
	if (-1 < d && d < 500) {
		int volume = volume_by_distance[d] - volume_attenuation;
		play_sound_buffer_2("bump2.wav", volume - (int)(speed * 25.0f), 0);
	}
}

void play_bump3_sound(float x, float y, float z)
{
	if (engine_sound_enabled == 0)
		return;
	int d = distance_to_player(x, y, z);
	if (-1 < d && d < 500)
		play_sound_buffer_2("bump3.wav", volume_by_distance[d] - volume_attenuation, 0);
}

void play_bump_sound(float x, float y, float z)
{
	if (engine_sound_enabled == 0)
		return;
	int d = distance_to_player(x, y, z);
	if (-1 < d && d < 500)
		play_sound_buffer_2("bump.wav", volume_by_distance[d] - volume_attenuation, 0);
}

/* --- the other cars' engines ------------------------------------------------------ */

static int find_free_sound_slot(void)
{
	for (int i = 0; i < 6; i++) {
		if (object_sounds[i].object == -1)
			return i;
	}
	return -1;
}

static int find_object_sound_slot(int object)
{
	for (int i = 0; i < 6; i++) {
		if (object_sounds[i].object == object)
			return i;
	}
	return -1;
}

/* The car type picks the sound: trucks, the police siren, or an ordinary engine. */
static void start_object_sound(int slot, int object, float distance)
{
	int op = objects3d[object].opponent;
	int volume = volume_by_distance[(int)distance] - volume_attenuation;
	if (engine_sound_enabled == 0)
		return;
	if (op < 0 || opponent_count < op) {
		fprintf(stderr, "start_object_sound: illegal opponent nr %d (max %d)\n", op, opponent_count);
		exit(1);
	}
	const char *name;
	switch (opponent_data[op].car_type) {
	case 1: name = "truck01.wav"; break;
	case 5: name = "truck02.wav"; break;
	case 11: name = "truck06.wav"; break;
	case 12: name = "sirene.wav"; break;
	default: name = "motor2.wav"; break;
	}
	object_sounds[slot].handle = play_sound_buffer_2(name, volume, SOUND_LOOPING);
	set_sound_buffer_vol(object_sounds[slot].handle,
		volume_by_distance[(int)distance] - volume_attenuation);
	object_sounds[slot].object = object;
}

static void set_object_sound_volume(int slot, float distance)
{
	if (engine_sound_enabled != 0)
		set_sound_buffer_vol(object_sounds[slot].handle,
			volume_by_distance[(int)distance] - volume_attenuation);
}

void stop_object_sound(int object)
{
	int slot = find_object_sound_slot(object);
	if (engine_sound_enabled != 0 && slot != -1) {
		stop_sound_buffer(object_sounds[slot].handle);
		object_sounds[slot].object = -1;
	}
}

void update_object_sound(int object, float x, float y, float z, float distance)
{
	(void)x;
	(void)y;
	(void)z;
	int slot = find_object_sound_slot(object);
	if (engine_sound_enabled == 0)
		return;
	if (distance < 0.0f || 500.0f < distance)
		distance = 0.0f;
	if (slot == -1) {
		slot = find_free_sound_slot();
		if (slot != -1)
			start_object_sound(slot, object, distance);
	}
	else {
		set_object_sound_volume(slot, distance);
	}
}

/* --- the player's engine (mycontro.cpp) ------------------------------------------- */

/* motor_samples: the low engine (0) and the high one (1) loop together, crossfading by
 * rpm; gearup and geardwn play on a change. */
static struct {
	const char *name;
	int handle;
} motor_samples[4] = {
	{ "motor.wav", -1 }, { "motor1.wav", -1 }, { "gearup.wav", -1 }, { "geardwn.wav", -1 },
};

/* By rpm (0..79): frequency and volume of the high engine, then of the low one. */
static struct {
	int freq, vol;
} engine_high[80], engine_low[80];

void init_engine_sound_tables(void)
{
	int i = 0;
	for (int r = 0; r < 35; r++, i++) {
		engine_high[r].freq = i + 0x2b11;
		engine_high[r].vol = r * 0x2d - 2000;
	}
	for (int r = 35; r < 80; r++, i += 400) {
		engine_high[r].vol = -0x203;
		engine_high[r].freq = i + 0x2b11;
	}
	for (int r = 0; r < 35; r++) {
		engine_low[r].vol = -100 - r * 0x2a;
		engine_low[r].freq = 0x157c;
	}
	for (int r = 35; r < 80; r++) {
		engine_low[r].freq = 0x157c;
		engine_low[r].vol = -5000;
	}
}

static void play_motor_sample(int sample, int loop)
{
	if (engine_sound_enabled != 0)
		motor_samples[sample].handle = play_sound_buffer_1(motor_samples[sample].name, loop);
}

static void load_and_play_motor_sample(int sample, int loop)
{
	if (engine_sound_enabled != 0) {
		int buffer = load_sound_buffer(motor_samples[sample].name, 4);
		motor_samples[sample].handle = buffer;
		play_sound_buffer(buffer, loop);
	}
}

void start_engine_sounds(void)
{
	if (engine_sound_enabled == 0)
		return;
	load_and_play_motor_sample(1, SOUND_LOOPING);
	load_and_play_motor_sample(0, SOUND_LOOPING);
	set_sound_buffer_freq(motor_samples[1].handle, engine_high[0].freq + 6000);
	set_sound_buffer_vol(motor_samples[1].handle,
		engine_high[0].vol + 300 - volume_attenuation - volume_offset);
	set_sound_buffer_vol(motor_samples[0].handle, engine_low[0].vol - volume_attenuation - volume_offset);
	set_sound_buffer_freq(motor_samples[0].handle, engine_low[0].freq);
}

void set_engine_sound(int rpm, int boost_freq, int boost_vol)
{
	if (engine_sound_enabled == 0)
		return;
	set_sound_buffer_freq(motor_samples[1].handle, engine_high[rpm].freq + boost_freq);
	set_sound_buffer_vol(motor_samples[1].handle,
		engine_high[rpm].vol + boost_vol - volume_attenuation - volume_offset);
	set_sound_buffer_freq(motor_samples[0].handle, engine_low[rpm].freq);
	set_sound_buffer_vol(motor_samples[0].handle, engine_low[rpm].vol - volume_attenuation - volume_offset);
}

void play_gear_change_sound(int new_gear, int old_gear)
{
	if (engine_sound_enabled == 0)
		return;
	int sample = old_gear < new_gear ? 2 : 3;
	play_motor_sample(sample, 0);
	set_sound_buffer_vol(motor_samples[sample].handle, -100 - volume_attenuation - volume_offset);
}
