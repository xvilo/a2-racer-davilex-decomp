/* State shared by the menu and (later) the race: loaded resources and options. */
#ifndef A2_GAME_H
#define A2_GAME_H

#include "res/archive.h"

typedef struct {
	const char *data_dir;      /* the original disc (or a copy of it) */
	named_archive menu_files;  /* A2MRCS: menu screens, sounds */
	sprite_archive menu_sprites; /* A2MRC: buttons, labels, anims */
	const char *screenshot;    /* write the first frame here and exit */
} game_t;

#endif
