/* optie.cpp and bedien.cpp. */
#include "menu/optie.h"

#include <stdio.h>
#include <string.h>

#include <SDL.h>

#include "xvga/fileio.h"

opties_t opties;
bedien_t control;
int available_joy;

void save_optie(void)
{
	if (file_write(&opties, sizeof opties, "OPTIE.A2R"))
		fprintf(stderr, "save_optie: cannot write %s\n", save_path("OPTIE.A2R"));
}

void init_optie(void)
{
	if (file_exist("OPTIE.A2R") && !file_read(&opties, sizeof opties, "OPTIE.A2R"))
		return;
	/* A missing or damaged file starts over with the defaults. */
	file_delete("OPTIE.A2R");
	memset(&opties, 0, sizeof opties);
	opties.view = 1;
	opties.resolution = 1;
	opties.sound = SOUND_ALL;
	opties.display_mode = 1;
	opties.horizon = 0;
	opties.dashboard = 0;
	opties.detail = 1;
	opties.camera = 0;
	opties.music_volume = 3;
	opties.effects_volume = 4;
	save_optie();
}

void save_bedien(void)
{
	if (file_write(&control, sizeof control, "BEDIEN.A2R"))
		fprintf(stderr, "save_bedien: cannot write %s\n", save_path("BEDIEN.A2R"));
}

void init_bedien(void)
{
	available_joy = SDL_WasInit(SDL_INIT_JOYSTICK) && SDL_NumJoysticks() > 0;
	if (file_exist("BEDIEN.A2R") && !file_read(&control, sizeof control, "BEDIEN.A2R"))
		return;
	file_delete("BEDIEN.A2R");
	memset(&control, 0, sizeof control);
	control.control_type = 0;
	control.field_08 = 1;
	control.key_left = 0x25;    /* VK_LEFT */
	control.key_right = 0x27;   /* VK_RIGHT */
	control.key_gas = 0x26;     /* VK_UP */
	control.key_brake = 0x28;   /* VK_DOWN */
	control.key_action1 = 0x20; /* VK_SPACE */
	control.key_action2 = 0x0d; /* VK_RETURN */
	save_bedien();
}
