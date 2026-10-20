/* The main menu (m_main.cpp). */
#ifndef A2_M_MAIN_H
#define A2_M_MAIN_H

#include "game.h"

/* menu_main's results. */
enum {
	MENU_START_RACE = 0x28, /* the launcher then runs the race */
	MENU_EXIT = 0xff,
};

/* Runs the main menu until the player starts a race or leaves. */
int menu_main(game_t *g);
/* Draws the main menu screen once without waiting for input (screenshots). */
void mm_open_screen(game_t *g);

#endif
