/* The race's sounds (sndfx.cpp, infio.cpp's effects, mycontro.cpp's engine): the music,
 * the spoken and bonus effects, the bumps and crashes by distance, the other cars'
 * engines and the player's two-layer engine with its gear changes. Volumes are
 * DirectSound's hundredths of a decibel. */
#ifndef A2_SNDFX_H
#define A2_SNDFX_H

extern int engine_sound_enabled, music_enabled;
/* From the options' volumes (init_race_settings). */
extern int volume_attenuation, volume_offset, music_attenuation;

/* Takes the sounds from RCS and fills the distance table (load_samples). */
void load_samples(void);
/* Stops everything and gives the sound back to the menu. */
void exit_samples(void);
/* One of the six tunes (index into the sample table, 2..7), looping. */
void start_music(int music);
/* Sample 0..0x2f of the table; with `loop`, waits until it has finished. */
void play_sound_effect(int effect, int loop);
/* At a position, quieter further from the player (and the crash louder harder). */
void play_crash_sound(float x, float y, float z, float speed);
void play_bump3_sound(float x, float y, float z);
void play_bump_sound(float x, float y, float z);

/* A car's engine (or siren) for the object, at `distance` from the player. */
void update_object_sound(int object, float x, float y, float z, float distance);
void stop_object_sound(int object);

/* mycontro.cpp: the engine's frequency and volume per rpm (init_controls). */
void init_engine_sound_tables(void);
void start_engine_sounds(void);
/* Pitch and volume of the engine for the gearbox step's rpm, plus the throttle boost. */
void set_engine_sound(int rpm, int boost_freq, int boost_vol);
void play_gear_change_sound(int new_gear, int old_gear);

#endif
