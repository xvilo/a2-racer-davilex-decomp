/* Platform layer: window, presenting the 8-bit screen, input and time. Replaces the
 * Win32 message loop and DirectDraw/DirectInput parts of the xvga library. */
#ifndef A2_SYS_H
#define A2_SYS_H

#include <stdbool.h>
#include <stdint.h>

enum {
	KEY_NONE = 0,
	KEY_ESCAPE = 27,
	KEY_ENTER = 13,
	KEY_SPACE = 32,
	KEY_UP = 0x100,
	KEY_DOWN,
	KEY_LEFT,
	KEY_RIGHT,
};

typedef struct {
	int mouse_x, mouse_y;  /* in 640x480 screen coordinates */
	bool mouse_down;       /* left button held */
	bool mouse_pressed;    /* left button went down since the last poll */
	bool mouse_clicked;    /* left button released since the last poll */
	int key;               /* last key pressed since the last poll, KEY_* or ASCII */
	char text[32];         /* characters typed since the last poll (Windows-1252) */
	int backspaces;        /* backspace presses since the last poll */
	bool quit;             /* window closed */
} sys_input;

typedef struct {
	int scale;              /* initial window size = 640x480 * scale */
	bool hidden;            /* no window (screenshots, tests) */
	const char *script;     /* scripted input, see sys_sdl.c (tests) */
} sys_options;

bool sys_init(const char *title, const sys_options *opt);
void sys_shutdown(void);
/* Shows an indexed 640x480 picture with the given palette. */
void sys_present(const uint8_t *pixels, const uint8_t palette[768]);
/* OpenGL drawing (the race). The 3D picture goes into a 640x480-sized frame that is
 * letterboxed into the window like the 2D screens. */
bool sys_gl(void);
void sys_gl_begin_frame(void);
/* Restricts drawing to a rectangle of the 640x480 frame. */
void sys_gl_viewport(int x, int y, int w, int h);
/* Draws an indexed 640x480 picture over the frame; index `transparent` stays see-through. */
void sys_gl_overlay(const uint8_t *pixels, const uint8_t palette[768], int transparent);
void sys_gl_swap(void);
/* The frame drawn so far (before the swap), RGB top-down; with rgb NULL only the size. */
void sys_gl_read_picture(uint8_t *rgb, int *w, int *h);
/* Gathers input since the last call. */
void sys_poll(sys_input *in);
/* The window was closed (or the process asked to stop); stays true from then on. */
bool sys_quit_requested(void);
uint32_t sys_ticks(void);
/* Whether a key (Windows virtual-key code, as the game stores them) is held now. */
bool keyispressed(int vk);
/* Typed characters arrive in sys_input.text while text input is on. */
void sys_text_input(bool on);
void sys_sleep(uint32_t ms);
/* The screen the script's "shot" command saves; set by the game. */
typedef void (*sys_screenshot_fn)(const char *path);
void sys_set_screenshot(sys_screenshot_fn fn);

#endif
