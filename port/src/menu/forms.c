#include "menu/forms.h"

#include <stdio.h>
#include <stdlib.h>

#include "gfx/screen.h"

#define MAX_CONTROLS 64

static anim_t *anim_table;
static form_t *activeform;
static int nmb_of_controls;
static int focus = -1;
static int pressed = -1;
static int state[MAX_CONTROLS];
static int drawn_frame[MAX_CONTROLS];
static uint32_t focus_anim_time;

void forms_init(const sprite_archive *menu_sprites, anim_t *anims)
{
	set_mem_images(menu_sprites);
	anim_table = anims;
}

int get_control_image_index(int anim, int frame)
{
	const anim_t *a = &anim_table[anim];
	if (frame < 0 || frame >= a->frame_count) {
		fprintf(stderr, "get_control_image_index: anim %d has no frame %d\n", anim, frame);
		frame = 0;
	}
	return a->frames[frame].image;
}

/* create_button: a size of -1 is taken from the button's first image. */
static void create_button(form_t *f)
{
	button_t *b = f->control;
	const sprite *s = get_mem_image(get_control_image_index(b->image, 0));
	if (b->width == -1 && s)
		b->width = s->width;
	if (b->height == -1 && s)
		b->height = s->height;
	b->focus_frame = 0;
}

static int inside(const form_t *f, int x, int y)
{
	const button_t *b = f->control;
	return x >= f->x && y >= f->y && x < f->x + b->width && y < f->y + b->height;
}

/* draw_button: the owner-draw handler. Frames of a button anim: 0 normal, 1 pressed,
 * 2 disabled, 3 focused. Drawn into the front screen, centred on the control. */
static void draw_button(int i)
{
	form_t *f = &activeform[i];
	button_t *b = f->control;
	int frame = 0;
	if (state[i] == STATE_DISABLED)
		frame = 2;
	else if (i == pressed)
		frame = 1;
	else if (i == focus)
		frame = 3;
	set_mem_image(SCREEN_FRONT, get_control_image_index(b->image, frame), f->x + b->width / 2,
		f->y + b->height / 2, MI_CLIP);
	drawn_frame[i] = frame;
}

/* draw_focus_anim_button: a second anim cycles on top of the focused button. */
static void draw_focus_anim_button(int i)
{
	form_t *f = &activeform[i];
	button_t *b = f->control;
	if (b->focus_anim == -1)
		return;
	set_mem_image(SCREEN_FRONT, get_control_image_index(b->focus_anim, b->focus_frame),
		f->x + b->width / 2, f->y + b->height / 2, MI_CLIP);
	if (b->focus_frame < anim_table[b->focus_anim].frame_count - 1)
		b->focus_frame++;
	else
		b->focus_frame = 0;
}

void activate_form(form_t *form)
{
	if (activeform)
		destroy_form();
	activeform = form;
	nmb_of_controls = 0;
	focus = -1;
	pressed = -1;
	for (form_t *f = form; f->type != -1 && nmb_of_controls < MAX_CONTROLS; f++, nmb_of_controls++) {
		int i = nmb_of_controls;
		if (f->type == CONTROL_BUTTON)
			create_button(f);
		/* set_control_state(i, state): 4 becomes 1; state 1 takes the focus. */
		state[i] = f->state == STATE_PRESSED ? STATE_FOCUS : f->state;
		if (state[i] == STATE_FOCUS && focus < 0)
			focus = i;
	}
	for (int i = 0; i < nmb_of_controls; i++) {
		if (activeform[i].type == CONTROL_BUTTON)
			draw_button(i);
	}
	focus_anim_time = sys_ticks();
}

void destroy_form(void)
{
	activeform = NULL;
	nmb_of_controls = 0;
}

int get_control_focus(void)
{
	return focus;
}

static void redraw_changed(void)
{
	for (int i = 0; i < nmb_of_controls; i++) {
		if (activeform[i].type != CONTROL_BUTTON)
			continue;
		int frame = state[i] == STATE_DISABLED ? 2 : i == pressed ? 1 : i == focus ? 3 : 0;
		if (frame != drawn_frame[i])
			draw_button(i);
	}
}

void focus_control(int index)
{
	if (index >= 0 && index < nmb_of_controls && state[index] != STATE_DISABLED) {
		focus = index;
		redraw_changed();
	}
}

void draw_form(int which)
{
	(void)which;
	for (int i = 0; i < nmb_of_controls; i++) {
		if (activeform[i].type == CONTROL_BUTTON)
			draw_button(i);
	}
}

int handle_form(const sys_input *in)
{
	if (!activeform)
		return FORM_NOTHING;
	if (in->quit)
		return FORM_QUIT;

	/* Windows moved the focus to the button under the mouse. */
	int hover = -1;
	for (int i = 0; i < nmb_of_controls; i++) {
		if (activeform[i].type == CONTROL_BUTTON && state[i] != STATE_DISABLED &&
			inside(&activeform[i], in->mouse_x, in->mouse_y))
			hover = i;
	}
	if (hover >= 0)
		focus = hover;
	pressed = in->mouse_down && hover >= 0 ? hover : -1;
	redraw_changed();

	/* draw_form_animfocus_buttons, every form_anim_timer calls. */
	if (focus >= 0 && sys_ticks() - focus_anim_time >= 66) {
		focus_anim_time = sys_ticks();
		draw_focus_anim_button(focus);
	}

	if (in->mouse_clicked && hover >= 0)
		return hover;
	/* check_form_key_pressed */
	switch (in->key) {
	case KEY_ESCAPE: return FORM_ESCAPE;
	case KEY_UP: return FORM_UP;
	case KEY_DOWN: return FORM_DOWN;
	case KEY_LEFT: return FORM_LEFT;
	case KEY_RIGHT: return FORM_RIGHT;
	case KEY_ENTER:
	case KEY_SPACE:
		return focus;
	default:
		break;
	}
	/* Hotkeys (the original needs Alt held). */
	for (int i = 0; in->key > 0 && in->key < 128 && i < nmb_of_controls; i++) {
		const button_t *b = activeform[i].control;
		if (activeform[i].type == CONTROL_BUTTON && b->hotkey > ' ' &&
			(b->hotkey | 0x20) == (in->key | 0x20))
			return i;
	}
	return FORM_NOTHING;
}
