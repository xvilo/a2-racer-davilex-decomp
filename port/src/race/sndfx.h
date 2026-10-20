/* The race's sounds (sndfx.cpp, sound.cpp, mycontro.cpp's engine). Not ported yet:
 * these are silent stand-ins with the original's entry points. */
#ifndef A2_SNDFX_H
#define A2_SNDFX_H

void play_sound_effect(int effect, int loop);
void play_bump_sound(void);
void play_bump3_sound(void);
void play_crash_sound(void);
void start_engine_sounds(void);
/* Pitch and volume of the engine for the gearbox step's rpm. */
void set_engine_sound(int rpm, int boost_freq, int boost_vol);
void play_gear_change_sound(int new_gear, int old_gear);

#endif
