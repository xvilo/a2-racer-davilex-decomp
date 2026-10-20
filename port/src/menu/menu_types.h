/* Menu table types. The layouts follow A2MENU.DAT's structures (symbols/structs.tsv),
 * without the original's packing or its run-time window handles. */
#ifndef A2_MENU_TYPES_H
#define A2_MENU_TYPES_H

typedef struct {
	int duration; /* ticks the frame stays up */
	int image;    /* sprite index in A2MRC */
} frame_t;

typedef struct {
	int frame_count;
	int mode;
	frame_t *frames;
} anim_t;

/* Control kinds in form_t.type. */
enum {
	CONTROL_BUTTON = 1,
};

/* One control on a form; a table ends with a row whose x is -1. */
typedef struct {
	int x, y;
	int type;
	void *control; /* the kind's own record, e.g. button_t */
	int state;     /* initial control state */
	int field_14;
} form_t;

typedef struct {
	int image;  /* anim index; frame 0 normal, 1 focused, 3 pressed */
	int width;  /* -1: from the image */
	int height; /* -1: from the image */
	char hotkey;
	int focus_anim; /* anim played while focused, -1 for none */
	int focus_frame;
} button_t;

/* Help text for a control; a table ends with control_id -1. */
typedef struct {
	int control_id;
	const char *text;
} help_t;

#endif
