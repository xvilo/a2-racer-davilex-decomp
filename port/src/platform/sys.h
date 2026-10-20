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
	bool mouse_clicked;    /* left button released since the last poll */
	int key;               /* last key pressed since the last poll, KEY_* or ASCII */
	bool quit;             /* window closed */
} sys_input;

typedef struct {
	int scale;              /* initial window size = 640x480 * scale */
	bool hidden;            /* no window (screenshots, tests) */
} sys_options;

bool sys_init(const char *title, const sys_options *opt);
void sys_shutdown(void);
/* Shows an indexed 640x480 picture with the given palette. */
void sys_present(const uint8_t *pixels, const uint8_t palette[768]);
/* Gathers input since the last call. */
void sys_poll(sys_input *in);
uint32_t sys_ticks(void);
void sys_sleep(uint32_t ms);

#endif
