/* Forms and their controls (forms.cpp, buttons.cpp, areabuts.cpp, editbox.cpp).
 *
 * The original made each control a Win32 child window: owner-drawn BUTTONs, STATIC
 * windows for clickable areas and EDIT windows for text. Windows gave them focus on a
 * click or Tab, pressed a button on Enter or a click, and sent the form a command. The
 * port keeps that behaviour and draws the controls itself, into the front screen, when
 * their state changes. */
#include "menu/forms.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx/screen.h"
#include "xvga/font.h"

#define MAX_CONTROLS 64
#define EDIT_MAX 256

static anim_t *anim_table;
static form_t *activeform;
static int nmb_of_controls;
static int focus = -1;
static int pressed = -1;
static int state[MAX_CONTROLS];
static int drawn[MAX_CONTROLS];  /* frame last drawn, -1 to force a redraw */
static char edit_text[MAX_CONTROLS][EDIT_MAX];
/* The initial text is selected, as when an EDIT gets the focus in a dialog: typing
 * replaces it. */
static int edit_selected[MAX_CONTROLS];
static int edit_font[MAX_CONTROLS];
static uint32_t focus_anim_time;
static uint32_t caret_time;

/* allowed_edit_keys: what an edit box accepts (clear_edit_keys_allowed). */
static char allowed_edit_keys[401] =
	"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890.<,/?![]%$&*+-=>:;' ";

void forms_init(const sprite_archive *menu_sprites, anim_t *anims)
{
	set_mem_images(menu_sprites);
	anim_table = anims;
}

void set_edit_keys_allowed(const char *keys)
{
	snprintf(allowed_edit_keys, sizeof allowed_edit_keys, "%s", keys);
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

static int is_edit(const form_t *f)
{
	return f->type == CONTROL_EDITBOX || f->type == CONTROL_EDITBOX_READONLY;
}

static void control_size(const form_t *f, int *w, int *h)
{
	*w = *h = 0;
	if (f->type == CONTROL_BUTTON) {
		const button_t *b = f->control;
		*w = b->width;
		*h = b->height;
	}
	else if (f->type == CONTROL_AREABUTTON) {
		const areabutton_t *a = f->control;
		*w = a->width;
		*h = a->height;
	}
	else if (is_edit(f)) {
		const edit_box_t *e = f->control;
		*w = e->width;
		*h = e->height;
	}
}

static int inside(const form_t *f, int x, int y)
{
	int w, h;
	control_size(f, &w, &h);
	return x >= f->x && y >= f->y && x < f->x + w && y < f->y + h;
}

/* --- buttons --------------------------------------------------------------------- */

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

/* Frames of a button anim: 0 normal, 1 pressed, 2 disabled, 3 focused (draw_button). */
static int button_frame(int i)
{
	return state[i] == STATE_DISABLED ? 2 : i == pressed ? 1 : i == focus ? 3 : 0;
}

static void draw_button(int i, int frame)
{
	form_t *f = &activeform[i];
	button_t *b = f->control;
	set_mem_image(SCREEN_FRONT, get_control_image_index(b->image, frame), f->x + b->width / 2,
		f->y + b->height / 2, MI_CLIP);
}

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

/* --- area buttons ---------------------------------------------------------------- */

static void create_areabutton(form_t *f)
{
	areabutton_t *a = f->control;
	if (a->image == -1)
		return;
	int image = get_control_image_index(a->image, 0);
	const sprite *s = get_mem_image(image);
	if (a->width == -1 && s)
		a->width = s->width;
	if (a->height == -1 && s)
		a->height = s->height;
	set_mem_image(SCREEN_FRONT, image, f->x + a->width / 2, f->y + a->height / 2, MI_CLIP);
}

/* --- edit boxes ------------------------------------------------------------------ */

static void create_edit_box(form_t *f, int i)
{
	edit_box_t *e = f->control;
	snprintf(edit_text[i], sizeof edit_text[i], "%s", e->text ? e->text : "");
	edit_selected[i] = edit_text[i][0] != 0;
	edit_font[i] = xvga_get_active_font();
	/* create_edit_box's colours: white on black, white on red while focused. */
	e->background = 0x000000;
	e->foreground = 0xffffff;
	e->focus_background = 0x0000ff;
	e->focus_foreground = 0xffffff;
}

/* The EDIT window: a filled box with the text one pixel in, and a caret when focused. */
static void draw_edit_box(int i)
{
	form_t *f = &activeform[i];
	edit_box_t *e = f->control;
	int focused = i == focus;
	uint32_t bg = focused ? e->focus_background : e->background;
	uint32_t fg = focused ? e->focus_foreground : e->foreground;
	int font = xvga_get_active_font();
	xvga_select_font(edit_font[i]);
	fill_rect_colorref(SCREEN_FRONT, f->x, f->y, e->width, e->height, bg);
	xvga_set_font_colors(edit_font[i], bg, fg);
	xvga_set_font_bkmode(edit_font[i], 1);
	if (focused && edit_selected[i]) {
		/* Selected text: inverted colours. */
		int w = get_string_width(edit_text[i], (int)strlen(edit_text[i]));
		fill_rect_colorref(SCREEN_FRONT, f->x + 1, f->y + 1, w, e->height - 2, fg);
		xvga_set_font_colors(edit_font[i], fg, bg);
	}
	win_print_xy(SCREEN_FRONT, f->x + 1, f->y, "%s", edit_text[i]);
	if (focused && (sys_ticks() / 500) % 2 == 0) {
		int x = f->x + 1 + get_string_width(edit_text[i], (int)strlen(edit_text[i]));
		fill_rect_colorref(SCREEN_FRONT, x, f->y + 2, 1, e->height - 4, fg);
	}
	xvga_select_font(font);
}

static void edit_box_input(int i, const sys_input *in)
{
	edit_box_t *e = activeform[i].control;
	char *t = edit_text[i];
	if (edit_selected[i] && (in->backspaces || in->text[0])) {
		t[0] = 0;
		edit_selected[i] = 0;
		if (!in->text[0])
			return;
	}
	size_t n = strlen(t);
	for (int b = 0; b < in->backspaces && n > 0; b++)
		t[--n] = 0;
	for (const char *c = in->text; *c; c++) {
		/* EditWindowProc only lets allowed_edit_keys through. */
		if (!strchr(allowed_edit_keys, *c) || (int)n >= e->max_chars || n + 1 >= EDIT_MAX)
			continue;
		t[n++] = *c;
		t[n] = 0;
	}
}

void get_edit_box_text(int index, char *buf, int size)
{
	if (index < 0 || index >= nmb_of_controls || !is_edit(&activeform[index])) {
		fprintf(stderr, "get_edit_box_text: control %d is not an edit box\n", index);
		abort();
	}
	snprintf(buf, (size_t)size, "%s", edit_text[index]);
}

/* --- forms ----------------------------------------------------------------------- */

static void redraw_changed(int force)
{
	for (int i = 0; i < nmb_of_controls; i++) {
		form_t *f = &activeform[i];
		if (f->type == CONTROL_BUTTON) {
			int frame = button_frame(i);
			if (force || frame != drawn[i]) {
				draw_button(i, frame);
				drawn[i] = frame;
			}
		}
		else if (is_edit(f)) {
			draw_edit_box(i);
		}
	}
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
		else if (f->type == CONTROL_AREABUTTON)
			create_areabutton(f);
		else if (is_edit(f))
			create_edit_box(f, i);
		/* set_control_state: 4 becomes 1; state 1 takes the focus. */
		state[i] = f->state == STATE_PRESSED ? STATE_FOCUS : f->state;
		if (state[i] == STATE_FOCUS && focus < 0)
			focus = i;
		drawn[i] = -1;
	}
	if (focus >= 0 && is_edit(&activeform[focus]))
		sys_text_input(true);
	redraw_changed(1);
	focus_anim_time = caret_time = sys_ticks();
}

void destroy_form(void)
{
	sys_text_input(false);
	activeform = NULL;
	nmb_of_controls = 0;
}

int get_control_focus(void)
{
	return focus;
}

int get_control_type(int index)
{
	return index >= 0 && index < nmb_of_controls ? activeform[index].type : -1;
}

int get_numberofcontrols_inform(void)
{
	return nmb_of_controls;
}

void focus_control(int index)
{
	if (index >= 0 && index < nmb_of_controls && state[index] != STATE_DISABLED) {
		focus = index;
		sys_text_input(is_edit(&activeform[index]));
		redraw_changed(0);
	}
}

/* set_next_control_focus / set_previous_control_focus (Tab, Shift+Tab). */
static void step_focus(int dir)
{
	for (int k = 1; k <= nmb_of_controls; k++) {
		int i = ((focus < 0 ? 0 : focus) + dir * k + nmb_of_controls) % nmb_of_controls;
		if (state[i] != STATE_DISABLED && activeform[i].type != CONTROL_AREABUTTON) {
			focus_control(i);
			return;
		}
	}
}

/* draw_help_button & co.: the control drawn undimmed while help talks about it. */
void call_help_function(int index)
{
	if (index < 0 || index >= nmb_of_controls)
		return;
	form_t *f = &activeform[index];
	if (f->type == CONTROL_BUTTON) {
		draw_button(index, state[index] == STATE_FOCUS || state[index] == STATE_PRESSED ? 1 : 0);
		drawn[index] = -1;
	}
	else {
		int w, h;
		control_size(f, &w, &h);
		blt_scr_2_scr(2, 0, f->x, f->y, f->x + w, f->y + h, 0);
	}
}

void draw_form(int which)
{
	(void)which;
	redraw_changed(1);
}

int handle_form(const sys_input *in)
{
	if (!activeform)
		return FORM_NOTHING;
	if (in->quit)
		return FORM_QUIT;

	int under = -1;
	for (int i = 0; i < nmb_of_controls; i++) {
		if (state[i] != STATE_DISABLED && inside(&activeform[i], in->mouse_x, in->mouse_y))
			under = i;
	}
	/* A button press takes the focus and shows the pressed frame until release. */
	if ((in->mouse_down || in->mouse_pressed) && under >= 0 && pressed < 0) {
		pressed = activeform[under].type == CONTROL_BUTTON ? under : -1;
		if (activeform[under].type != CONTROL_AREABUTTON)
			focus_control(under);
	}
	int clicked = -1;
	if (in->mouse_clicked) {
		if (under >= 0 && (pressed == under || activeform[under].type == CONTROL_AREABUTTON))
			clicked = under;
		pressed = -1;
	}
	if (!in->mouse_down)
		pressed = -1;

	if (focus >= 0 && is_edit(&activeform[focus]))
		edit_box_input(focus, in);
	redraw_changed(0);

	/* draw_form_animfocus_buttons, every form_anim_timer calls. */
	if (focus >= 0 && activeform[focus].type == CONTROL_BUTTON &&
		sys_ticks() - focus_anim_time >= 66) {
		focus_anim_time = sys_ticks();
		draw_focus_anim_button(focus);
	}

	if (clicked >= 0 && !is_edit(&activeform[clicked]))
		return clicked;
	/* check_form_key_pressed */
	switch (in->key) {
	case KEY_ESCAPE: return FORM_ESCAPE;
	case KEY_UP: return FORM_UP;
	case KEY_DOWN: return FORM_DOWN;
	case KEY_LEFT: return FORM_LEFT;
	case KEY_RIGHT: return FORM_RIGHT;
	case '\t':
		step_focus(keyispressed(0x10) ? -1 : 1);
		return FORM_NOTHING;
	case KEY_ENTER:
		/* Enter presses the focused button; in an edit box the requester checks it. */
		return focus >= 0 && activeform[focus].type == CONTROL_BUTTON ? focus : FORM_ENTER;
	case KEY_SPACE:
		if (focus >= 0 && activeform[focus].type == CONTROL_BUTTON)
			return focus;
		break;
	default:
		break;
	}
	/* Alt + hotkey presses the control with that hotkey. */
	if (keyispressed(0x12) && in->key > 0 && in->key < 128) {
		for (int i = 0; i < nmb_of_controls; i++) {
			char hotkey = 0;
			if (activeform[i].type == CONTROL_BUTTON)
				hotkey = ((const button_t *)activeform[i].control)->hotkey;
			else if (activeform[i].type == CONTROL_AREABUTTON)
				hotkey = ((const areabutton_t *)activeform[i].control)->hotkey;
			if (hotkey > ' ' && (hotkey | 0x20) == (in->key | 0x20))
				return i;
		}
	}
	return FORM_NOTHING;
}

/* wait_form_key_released: until the mouse button and Enter are up. */
void wait_form_key_released(void)
{
	sys_input in = { 0 };
	do {
		sys_poll(&in);
		sys_sleep(5);
	} while ((in.mouse_down || keyispressed(0x0d) || keyispressed(0x20)) && !in.quit);
}
