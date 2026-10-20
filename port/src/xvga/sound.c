#include "xvga/sound.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#define MAX_BUFFERS 64
#define MAX_SAMPLES 64

/* Decoded WAV data in the device's format (shared like DuplicateSoundBuffer does). */
typedef struct {
	char name[40];
	Uint8 *data;
	Uint32 len;
	int rate;        /* the WAV's own rate, for set_sound_buffer_freq */
} sample_t;

/* snd_buf_t in the original: name, DirectSound buffers, in-use flag, usage count. */
typedef struct {
	char name[40];
	int sample;      /* index into samples, -1 when the buffer is free */
	double pos;      /* in frames */
	int freq;        /* playback rate in Hz, 0 for the WAV's own */
	int playing;
	int looping;
	int volume;      /* hundredths of a dB */
	int use_count;   /* how often played; the least used buffer is freed first */
} snd_buf_t;

static SDL_AudioDeviceID device;
static SDL_AudioSpec spec;
static const named_archive *archive;
static sample_t samples[MAX_SAMPLES];
static int sample_count;
static snd_buf_t buffers[MAX_BUFFERS];
static int max_sound_buffers;
static int init_sound_ok;

static void mix(void *user, Uint8 *stream, int len)
{
	Sint16 *out = (Sint16 *)stream;
	int frames = len / 4;
	memset(stream, 0, (size_t)len);
	for (int b = 0; b < max_sound_buffers; b++) {
		snd_buf_t *buf = &buffers[b];
		if (!buf->playing || buf->sample < 0)
			continue;
		const sample_t *s = &samples[buf->sample];
		/* DirectSound volume: attenuation in hundredths of a decibel. */
		float gain = buf->volume <= -10000 ? 0.0f : powf(10.0f, buf->volume / 2000.0f);
		/* The data is at the device's rate; a set frequency plays it faster or slower. */
		double step = buf->freq > 0 && s->rate > 0 ? (double)buf->freq / s->rate : 1.0;
		Uint32 count = s->len / 4;
		const Sint16 *in = (const Sint16 *)s->data;
		for (int i = 0; i < frames; i++) {
			if (buf->pos >= count) {
				if (!buf->looping || count == 0) {
					buf->playing = 0;
					break;
				}
				buf->pos -= count;
				if (buf->pos >= count)
					buf->pos = 0;
			}
			Uint32 a = (Uint32)buf->pos;
			Uint32 b = a + 1 < count ? a + 1 : buf->looping ? 0 : a;
			float t = (float)(buf->pos - a);
			for (int c = 0; c < 2; c++) {
				float x = in[a * 2 + c] + (in[b * 2 + c] - in[a * 2 + c]) * t;
				int v = out[i * 2 + c] + (int)(x * gain);
				out[i * 2 + c] = (Sint16)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
			}
			buf->pos += step;
		}
	}
}

int xvga_init_sound(int max_buffers, const named_archive *files)
{
	archive = files;
	max_sound_buffers = max_buffers < MAX_BUFFERS ? max_buffers : MAX_BUFFERS;
	for (int i = 0; i < MAX_BUFFERS; i++)
		buffers[i].sample = -1;
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
		return -1;
	SDL_AudioSpec want = { 0 };
	want.freq = 22050; /* the original asked for 22050 Hz, 16-bit stereo first */
	want.format = AUDIO_S16SYS;
	want.channels = 2;
	want.samples = 1024;
	want.callback = mix;
	device = SDL_OpenAudioDevice(NULL, 0, &want, &spec, 0);
	if (!device) {
		fprintf(stderr, "sound: %s\n", SDL_GetError());
		return -1;
	}
	SDL_PauseAudioDevice(device, 0);
	init_sound_ok = 1;
	return 0;
}

void xvga_exit_sound(void)
{
	if (device)
		SDL_CloseAudioDevice(device);
	device = 0;
	for (int i = 0; i < sample_count; i++)
		SDL_free(samples[i].data);
	sample_count = 0;
	init_sound_ok = 0;
}

static int name_equal_ci(const char *a, const char *b)
{
	return SDL_strcasecmp(a, b) == 0;
}

/* load_wave_file: the WAV from the archive, converted to the device format. */
static int load_sample(const char *filename)
{
	for (int i = 0; i < sample_count; i++) {
		if (name_equal_ci(samples[i].name, filename))
			return i;
	}
	if (sample_count == MAX_SAMPLES)
		return -1;
	size_t size;
	const uint8_t *data = named_archive_find(archive, filename, &size);
	if (!data) {
		fprintf(stderr, "sound: %s not found\n", filename);
		return -1;
	}
	SDL_AudioSpec wav;
	Uint8 *buf;
	Uint32 len;
	if (!SDL_LoadWAV_RW(SDL_RWFromConstMem(data, (int)size), 1, &wav, &buf, &len)) {
		fprintf(stderr, "sound: %s: %s\n", filename, SDL_GetError());
		return -1;
	}
	SDL_AudioCVT cvt;
	SDL_BuildAudioCVT(&cvt, wav.format, wav.channels, wav.freq, AUDIO_S16SYS, 2, spec.freq);
	cvt.len = (int)len;
	cvt.buf = SDL_malloc((size_t)len * (cvt.len_mult > 0 ? cvt.len_mult : 1));
	memcpy(cvt.buf, buf, len);
	SDL_FreeWAV(buf);
	if (cvt.needed)
		SDL_ConvertAudio(&cvt);
	sample_t *s = &samples[sample_count];
	snprintf(s->name, sizeof s->name, "%s", filename);
	s->data = cvt.buf;
	s->len = cvt.needed ? (Uint32)cvt.len_cvt : len;
	s->rate = wav.freq;
	return sample_count++;
}

static int search_sound_buffer(const char *filename, int start, int idle_only)
{
	for (int i = start; i < max_sound_buffers; i++) {
		if (buffers[i].sample >= 0 && name_equal_ci(buffers[i].name, filename) &&
			(!idle_only || !buffers[i].playing))
			return i;
	}
	return -1;
}

static int get_free_buffer(void)
{
	for (int i = 0; i < max_sound_buffers; i++) {
		if (buffers[i].sample < 0)
			return i;
	}
	return -1;
}

/* delete_sound_buffer(-1): frees the least used idle buffer other than `keep`. */
static int delete_sound_buffer(int keep)
{
	int best = -1;
	for (int i = 0; i < max_sound_buffers; i++) {
		if (i == keep || buffers[i].playing)
			continue;
		if (best < 0 || buffers[i].use_count < buffers[best].use_count)
			best = i;
	}
	if (best >= 0)
		buffers[best].sample = -1;
	return best;
}

/* Finds or makes a buffer for the file (duplicating one still playing) and starts it;
 * the device stays locked. */
static int start_buffer(const char *filename, int flags, int volume, int play)
{
	int b = search_sound_buffer(filename, 0, 0);
	if (b >= 0 && buffers[b].playing) {
		/* Still playing: use another idle copy of the same sound, or duplicate it. */
		int idle = search_sound_buffer(filename, b + 1, 1);
		b = idle >= 0 ? idle : -1;
	}
	if (b < 0) {
		b = get_free_buffer();
		if (b < 0)
			b = delete_sound_buffer(-1);
		int s = b < 0 ? -1 : load_sample(filename);
		if (s < 0)
			return -1;
		snprintf(buffers[b].name, sizeof buffers[b].name, "%s", filename);
		buffers[b].sample = s;
		buffers[b].use_count = 0;
		buffers[b].volume = 0;
		buffers[b].freq = 0;
	}
	buffers[b].use_count = flags & 4 ? 1000000 : buffers[b].use_count + 1;
	if (volume != 1)
		buffers[b].volume = volume;
	buffers[b].pos = 0;
	buffers[b].looping = flags & SOUND_LOOPING;
	buffers[b].playing = play;
	return b;
}

int play_sound_buffer_1(const char *filename, int flags)
{
	if (!init_sound_ok)
		return -1;
	SDL_LockAudioDevice(device);
	int b = start_buffer(filename, flags, 1, 1);
	SDL_UnlockAudioDevice(device);
	return b;
}

int play_sound_buffer_2(const char *filename, int volume, int flags)
{
	if (!init_sound_ok)
		return -1;
	if (volume < -10000)
		volume = -10000;
	if (volume > 0)
		volume = 0;
	SDL_LockAudioDevice(device);
	int b = start_buffer(filename, flags, volume, 1);
	SDL_UnlockAudioDevice(device);
	return b;
}

int load_sound_buffer(const char *filename, int flags)
{
	if (!init_sound_ok)
		return -1;
	SDL_LockAudioDevice(device);
	int b = start_buffer(filename, flags, 1, 0);
	SDL_UnlockAudioDevice(device);
	return b;
}

int play_sound_buffer(int index, int flags)
{
	if (!init_sound_ok || index < 0 || index >= max_sound_buffers)
		return 0;
	SDL_LockAudioDevice(device);
	if (buffers[index].sample >= 0) {
		buffers[index].pos = 0;
		buffers[index].looping = flags & SOUND_LOOPING;
		buffers[index].playing = 1;
	}
	SDL_UnlockAudioDevice(device);
	return 0;
}

int set_sound_buffer_freq(int index, int freq)
{
	if (!init_sound_ok || index < 0 || index >= max_sound_buffers)
		return 0;
	if (freq < 100 || 100000 < freq) {
		fprintf(stderr, "set_sound_buffer_freq: attempt at setting buffer %d to %d Hz\n", index, freq);
		return 0;
	}
	buffers[index].freq = freq;
	return 0;
}

void xvga_set_sound_files(const named_archive *files, int max_buffers)
{
	if (!init_sound_ok) {
		archive = files;
		return;
	}
	SDL_LockAudioDevice(device);
	for (int i = 0; i < MAX_BUFFERS; i++) {
		buffers[i].sample = -1;
		buffers[i].playing = 0;
	}
	for (int i = 0; i < sample_count; i++)
		SDL_free(samples[i].data);
	sample_count = 0;
	archive = files;
	max_sound_buffers = max_buffers < MAX_BUFFERS ? max_buffers : MAX_BUFFERS;
	SDL_UnlockAudioDevice(device);
}

int sound_buffer_busy(int index)
{
	if (!init_sound_ok || index < 0 || index >= max_sound_buffers)
		return 0;
	SDL_LockAudioDevice(device);
	int busy = buffers[index].sample >= 0 && buffers[index].playing;
	SDL_UnlockAudioDevice(device);
	return busy;
}

int stop_sound_buffer(int index)
{
	if (!init_sound_ok || index < 0 || index >= max_sound_buffers)
		return 0;
	SDL_LockAudioDevice(device);
	buffers[index].playing = 0;
	buffers[index].pos = 0;
	SDL_UnlockAudioDevice(device);
	return 0;
}

void stop_all_sound_buffers(void)
{
	for (int i = 0; i < max_sound_buffers; i++)
		stop_sound_buffer(i);
}

int set_sound_buffer_vol(int index, int volume)
{
	if (!init_sound_ok || index < 0 || index >= max_sound_buffers)
		return 0;
	if (volume < -10000)
		volume = -10000;
	if (volume > 0)
		volume = 0;
	buffers[index].volume = volume;
	return 0;
}

int get_sound_buffer_vol(int index)
{
	if (!init_sound_ok || index < 0 || index >= max_sound_buffers)
		return -10000;
	return buffers[index].volume;
}

int max_sound_buffer_count(void)
{
	return max_sound_buffers;
}

/* PCM from elsewhere (the movies' sound): one sample slot, replaced on each call. */
int play_pcm(const int16_t *pcm, size_t frames, int rate, int channels, int volume)
{
	if (!init_sound_ok || !frames)
		return -1;
	SDL_AudioCVT cvt;
	SDL_BuildAudioCVT(&cvt, AUDIO_S16SYS, (Uint8)channels, rate, AUDIO_S16SYS, 2, spec.freq);
	size_t len = frames * (size_t)channels * 2;
	cvt.len = (int)len;
	cvt.buf = SDL_malloc(len * (size_t)(cvt.len_mult > 0 ? cvt.len_mult : 1));
	memcpy(cvt.buf, pcm, len);
	if (cvt.needed)
		SDL_ConvertAudio(&cvt);
	SDL_LockAudioDevice(device);
	int si = -1;
	for (int i = 0; i < sample_count; i++) {
		if (!strcmp(samples[i].name, "<pcm>"))
			si = i;
	}
	if (si < 0 && sample_count < MAX_SAMPLES)
		si = sample_count++;
	int b = si < 0 ? -1 : get_free_buffer();
	if (b < 0 && si >= 0)
		b = delete_sound_buffer(-1);
	if (b < 0) {
		SDL_UnlockAudioDevice(device);
		SDL_free(cvt.buf);
		return -1;
	}
	/* Stop any buffer still playing the old PCM before its data goes. */
	for (int i = 0; i < max_sound_buffers; i++) {
		if (buffers[i].sample == si) {
			buffers[i].playing = 0;
			buffers[i].sample = -1;
		}
	}
	SDL_free(samples[si].data);
	snprintf(samples[si].name, sizeof samples[si].name, "<pcm>");
	samples[si].data = cvt.buf;
	samples[si].len = cvt.needed ? (Uint32)cvt.len_cvt : (Uint32)len;
	samples[si].rate = rate;
	snprintf(buffers[b].name, sizeof buffers[b].name, "<pcm>");
	buffers[b].sample = si;
	buffers[b].pos = 0;
	buffers[b].looping = 0;
	buffers[b].freq = 0;
	buffers[b].volume = volume;
	buffers[b].use_count = 1000000;
	buffers[b].playing = 1;
	SDL_UnlockAudioDevice(device);
	return b;
}
