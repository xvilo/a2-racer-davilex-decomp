/* Spoken help (help.cpp): the screen is dimmed with a grid and each control is shown
 * while its voice clip plays. */
#ifndef A2_HELP_H
#define A2_HELP_H

#include "menu/menu_types.h"

/* Dims a screen: every other pixel in a checkerboard becomes palette index 0. */
void draw_grid(int which);
/* Plays the help for the active form; `save` is the screen that keeps the picture. */
void play_help(const help_t *help, int save);
void set_help_key(int vk);
void clear_help_keys(void);

#endif
