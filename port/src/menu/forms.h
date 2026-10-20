/* Menu forms: a table of controls drawn on the menu screen (forms.cpp, buttons.cpp).
 *
 * The original made each control a Win32 child window with owner drawing; the port
 * draws them into the screen itself and does its own hit testing and focus handling,
 * with the same results from handle_form. */
#ifndef A2_FORMS_H
#define A2_FORMS_H

#include "menu/menu_types.h"
#include "platform/sys.h"
#include "res/archive.h"

/* handle_form results besides a control index. */
enum {
	FORM_NOTHING = -1,
	FORM_QUIT = -2,       /* window closed */
	FORM_ESCAPE = 10000,
	FORM_UP = 10001,
	FORM_DOWN = 10002,
	FORM_LEFT = 10003,
	FORM_RIGHT = 10004,
};

/* Control states (set_control_state). */
enum {
	STATE_NORMAL = 0,
	STATE_FOCUS = 1,
	STATE_DISABLED = 2,
	STATE_DEFAULT = 3, /* table value: normal until drawn */
	STATE_PRESSED = 4,
};

void forms_init(const sprite_archive *menu_sprites, anim_t *anims);
void activate_form(form_t *form);
void destroy_form(void);
/* Processes input once and redraws the form; returns a control index, FORM_* or
 * FORM_NOTHING. */
int handle_form(const sys_input *in);
int get_control_focus(void);
void focus_control(int index);
/* Sprite index for frame `frame` of anim `anim` (get_control_image_index). */
int get_control_image_index(int anim, int frame);
/* Draws the active form into the given screen. */
void draw_form(int which);

#endif
