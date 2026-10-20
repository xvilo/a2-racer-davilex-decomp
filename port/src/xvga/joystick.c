#include "xvga/joystick.h"

#include <stdio.h>
#include <stdlib.h>

#include "platform/sys.h"

/* joystickstate: 0x5c bytes a joystick; entry 0 is unused. */
typedef struct {
	int attached;
	struct {
		int key;
		int down;
	} keys[8];
	int x_base, y_base; /* the centre */
	int x_max, x_min, y_max, y_min;
} joystick_state_t;

static joystick_state_t joystickstate[3];
static double joysticksencesibility = 0.1;

int get_attached_joysticks(void)
{
	unsigned x, y, buttons;
	joystickstate[1].attached = sys_joystick_position(0, &x, &y, &buttons);
	joystickstate[2].attached = sys_joystick_position(1, &x, &y, &buttons);
	return (joystickstate[1].attached ? 1 : 0) + (joystickstate[2].attached ? 2 : 0);
}

void calibrate_joysticks(void)
{
	for (int j = 1; j <= 2; j++) {
		unsigned x, y, buttons;
		joystick_state_t *s = &joystickstate[j];
		s->attached = sys_joystick_position(j - 1, &x, &y, &buttons);
		if (s->attached) {
			s->x_base = (int)x;
			s->y_base = (int)y;
		}
		/* joyGetDevCaps: SDL's axes give the whole range. */
		s->x_min = s->y_min = 0;
		s->x_max = s->y_max = 0xffff;
	}
}

static void clear_joystick_key_down_state(int joystick)
{
	for (int i = 0; i < 8; i++)
		joystickstate[joystick].keys[i].down = 0;
}

static void check_joystick_movement(int joystick, int x, int y, unsigned buttons)
{
	joystick_state_t *s = &joystickstate[joystick];
	clear_joystick_key_down_state(joystick);
	if (s->x_base * joysticksencesibility + s->x_base < (double)x)
		s->keys[1].down = 1;
	if ((double)x < s->x_base - s->x_base * joysticksencesibility)
		s->keys[0].down = 1;
	if (s->y_base * joysticksencesibility + s->y_base < (double)y)
		s->keys[3].down = 1;
	if ((double)y < s->y_base - s->y_base * joysticksencesibility)
		s->keys[2].down = 1;
	for (int i = 0; i < 4; i++) {
		if (buttons & (1u << i))
			s->keys[4 + i].down = 1;
	}
}

void handle_joystick(void)
{
	for (int j = 1; j <= 2; j++) {
		if (!joystickstate[j].attached)
			continue;
		unsigned x, y, buttons;
		if (sys_joystick_position(j - 1, &x, &y, &buttons))
			check_joystick_movement(j, (int)x, (int)y, buttons);
		else
			clear_joystick_key_down_state(j);
	}
}

void set_joystick_keys(int joystick, int slot, int key)
{
	if (joystick == 0 || 2 < joystick) {
		fprintf(stderr, "set_joystick_keys: invalid joystick %d\n", joystick);
		exit(1);
	}
	if (slot < 0 || 7 < slot) {
		fprintf(stderr, "set_joystick_keys: invalid joystick key %d\n", slot);
		exit(1);
	}
	joystickstate[joystick].keys[slot].key = key;
}

void set_joystick_sensibility(double sensibility)
{
	if (1.0 < sensibility || sensibility < 0.05) {
		fprintf(stderr, "set_joystick_sencesibility: invalid value\n");
		exit(1);
	}
	joysticksencesibility = sensibility;
}

int get_joystick_position(int joystick, int width, int height, int *x, int *y, int *buttons)
{
	int half_w = width / 2, half_h = height / 2;
	if (joystick != 1 && joystick != 2) {
		fprintf(stderr, "get_joystick_position: invalid joystick %d\n", joystick);
		exit(1);
	}
	joystick_state_t *s = &joystickstate[joystick];
	if (!s->attached)
		return 0;
	*buttons = 0;
	unsigned jx, jy, jb;
	if (!sys_joystick_position(joystick - 1, &jx, &jy, &jb))
		return 0;
	double c = s->x_base, d = s->x_base * joysticksencesibility;
	if (c + d < (double)jx) {
		*x = (int)(jx / (unsigned)((s->x_max - s->x_base) / half_w));
		*buttons |= 8;
	}
	else if ((double)jx < c - d) {
		*x = (int)(jx / (unsigned)((s->x_base - s->x_min) / half_w));
		*buttons |= 4;
	}
	else {
		*x = half_w;
	}
	c = s->y_base;
	d = s->y_base * joysticksencesibility;
	if (c + d < (double)jy) {
		*y = (int)(jy / (unsigned)((s->y_max - s->y_base) / half_h));
		*buttons |= 2;
	}
	else if ((double)jy < c - d) {
		*y = (int)(jy / (unsigned)((s->y_base - s->y_min) / half_h));
		*buttons |= 1;
	}
	else {
		*y = half_h;
	}
	return 1;
}

int check_joystick_keys(int key)
{
	for (int j = 1; j <= 2; j++) {
		for (int i = 0; i < 8; i++) {
			if (joystickstate[j].keys[i].key == key && joystickstate[j].keys[i].down)
				return 1;
		}
	}
	return 0;
}

int keyispressed_1(int key, int device)
{
	if (device < 0 || 3 < device) {
		fprintf(stderr, "keyispressed: illegal device request %d\n", device);
		exit(1);
	}
	if ((device & 2) && check_joystick_keys(key))
		return -1;
	if ((device & 1) && keyispressed(key))
		return -1;
	return 0;
}
