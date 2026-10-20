#include "menu/forms.h"

#include <stdio.h>
#include <stdlib.h>

#include "gfx/screen.h"

#define MAX_CONTROLS 64

static const sprite_archive *sprites;
static anim_t *anim_table;
static form_t *active;
static int count;
static int focus = -1;
static int pressed = -1;
static int state[MAX_CONTROLS];
static uint32_t focus_anim_time;

void forms_init(const sprite_archive *menu_sprites, anim_t *anims)
{
	sprites = menu_sprites;
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

static const sprite *image(int index)
{
	return index >= 0 && index < sprites->count ? &sprites->sprites[index] : NULL;
}

/* create_button: a size of -1 is taken from the button's first image. */
static void create_button(form_t *f)
{
	button_t *b = f->control;
	const sprite *s = image(get_control_image_index(b->image, 0));
	if (b->width == -1 && s)
		b->width = s->width;
	if (b->height == -1 && s)
		b->height = s->height;
	b->focus_frame = 0;
}

static bool inside(const form_t *f, int x, int y)
{
	const button_t *b = f->control;
	return x >= f->x && y >= f->y && x < f->x + b->width && y < f->y + b->height;
}

void activate_form(form_t *form)
{
	active = form;
	count = 0;
	focus = -1;
	pressed = -1;
	for (form_t *f = form; f->type != -1 && count < MAX_CONTROLS; f++, count++) {
		if (f->type == CONTROL_BUTTON)
			create_button(f);
		/* A table state of 4 starts out as 1 (create_button). */
		state[count] = f->state == STATE_PRESSED ? STATE_FOCUS : f->state;
		if (state[count] == STATE_FOCUS && focus < 0)
			focus = count;
	}
	focus_anim_time = sys_ticks();
}

void destroy_form(void)
{
	active = NULL;
	count = 0;
}

int get_control_focus(void)
{
	return focus;
}

void focus_control(int index)
{
	if (index >= 0 && index < count && state[index] != STATE_DISABLED)
		focus = index;
}

/* Frames of a button anim: 0 normal, 1 pressed, 2 disabled, 3 focused (draw_button). */
static void draw_button(int which, int i)
{
	form_t *f = &active[i];
	button_t *b = f->control;
	int frame = 0;
	if (state[i] == STATE_DISABLED)
		frame = 2;
	else if (i == pressed)
		frame = 1;
	else if (i == focus)
		frame = 3;
	screen_blit_sprite(which, image(get_control_image_index(b->image, frame)), f->x, f->y);
	/* draw_focus_anim_button: a second anim cycles on top of the focused button. */
	if (i == focus && b->focus_anim != -1) {
		const sprite *s = image(get_control_image_index(b->focus_anim, b->focus_frame));
		if (s)
			screen_blit_sprite(which, s, f->x + b->width / 2 - s->center_x,
				f->y + b->height / 2 - s->center_y);
	}
}

void draw_form(int which)
{
	for (int i = 0; i < count; i++) {
		if (active[i].type == CONTROL_BUTTON)
			draw_button(which, i);
	}
}

int handle_form(const sys_input *in)
{
	if (!active)
		return FORM_NOTHING;
	if (in->quit)
		return FORM_QUIT;

	/* The focus follows the mouse over a control. */
	int hover = -1;
	for (int i = 0; i < count; i++) {
		if (active[i].type == CONTROL_BUTTON && state[i] != STATE_DISABLED &&
			inside(&active[i], in->mouse_x, in->mouse_y))
			hover = i;
	}
	if (hover >= 0)
		focus = hover;
	pressed = in->mouse_down && hover >= 0 ? hover : -1;

	/* Focus animation, advanced every few ticks like form_anim_timer. */
	if (focus >= 0 && sys_ticks() - focus_anim_time >= 66) {
		button_t *b = active[focus].control;
		focus_anim_time = sys_ticks();
		if (b->focus_anim != -1)
			b->focus_frame = (b->focus_frame + 1) % anim_table[b->focus_anim].frame_count;
	}

	if (in->mouse_clicked && hover >= 0)
		return hover;
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
	for (int i = 0; in->key > 0 && in->key < 128 && i < count; i++) {
		const button_t *b = active[i].control;
		if (active[i].type == CONTROL_BUTTON && b->hotkey > ' ' &&
			(b->hotkey | 0x20) == (in->key | 0x20))
			return i;
	}
	return FORM_NOTHING;
}
