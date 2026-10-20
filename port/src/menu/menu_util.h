/* Helpers shared by the menus. */
#ifndef A2_MENU_UTIL_H
#define A2_MENU_UTIL_H

#include "game.h"

/* load_pcx_file_1 flags (rcpcx.cpp): 1 the picture, 2 read the palette, 4 apply it. */
enum { PCX_IMAGE = 1, PCX_PALETTE = 2, PCX_SET_PALETTE = 4 };

/* Loads a menu picture from A2MRCS into a screen; returns 0, or 1000 if missing. */
int load_pcx_file_1(game_t *g, int which, const char *name, int flags);

#endif
