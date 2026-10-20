#include "menu/mysound.h"

#include "menu/optie.h"
#include "platform/sys.h"
#include "xvga/sound.h"

int mysnd_state;

static int snd_valid_buffer(int handle)
{
	return handle >= 0 && handle < max_sound_buffer_count();
}

int snd_volume_for_level(int level)
{
	return (4 - level) * -500 - 100;
}

void snd_stop(int *handle)
{
	if (snd_valid_buffer(*handle) && sound_buffer_busy(*handle))
		stop_sound_buffer(*handle);
	*handle = -1;
}

void snd_play(int *handle, const char *filename)
{
	if (mysnd_state == 1) {
		*handle = -1;
		return;
	}
	if (opties.sound != SOUND_OFF) {
		snd_stop(handle);
		*handle = play_sound_buffer_1(filename, 0);
		snd_set_volume(handle, snd_volume_for_level(opties.effects_volume));
	}
}

void snd_play_looping(int *handle, const char *filename)
{
	if (mysnd_state == 1) {
		*handle = -1;
		return;
	}
	if (opties.sound != SOUND_OFF) {
		snd_stop(handle);
		*handle = play_sound_buffer_1(filename, SOUND_LOOPING);
		snd_set_volume(handle, snd_volume_for_level(opties.music_volume));
	}
}

void snd_wait(int *handle)
{
	if (snd_valid_buffer(*handle) && opties.sound != SOUND_OFF) {
		sys_input in = { 0 };
		while (sound_buffer_busy(*handle)) {
			sys_poll(&in); /* handle_win_msg */
			if (in.quit)
				break;
			sys_sleep(5);
		}
	}
	snd_stop(handle);
}

int snd_fade_out(int *handle)
{
	if (snd_valid_buffer(*handle)) {
		if (opties.sound == SOUND_OFF) {
			snd_stop(handle);
		}
		else if (sound_buffer_busy(*handle)) {
			int v = get_sound_buffer_vol(*handle);
			if (0 < v - 1) {
				set_sound_buffer_vol(*handle, v - 1);
				return 1;
			}
		}
	}
	snd_stop(handle);
	return 0;
}

int snd_set_volume(int *handle, int volume)
{
	if (snd_valid_buffer(*handle)) {
		if (opties.sound != SOUND_OFF) {
			set_sound_buffer_vol(*handle, volume);
			return 1;
		}
		snd_stop(handle);
	}
	snd_stop(handle);
	return 0;
}

int snd_get_volume(int *handle)
{
	if (snd_valid_buffer(*handle)) {
		if (opties.sound != SOUND_OFF)
			return get_sound_buffer_vol(*handle);
		snd_stop(handle);
	}
	return -8000;
}
