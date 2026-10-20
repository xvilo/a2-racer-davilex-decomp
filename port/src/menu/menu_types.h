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
	CONTROL_EDITBOX = 0,
	CONTROL_BUTTON = 1,
	CONTROL_EDITBOX_READONLY = 3,
	CONTROL_AREABUTTON = 4,
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
	int image;  /* anim index; frame 0 normal, 1 pressed, 2 disabled, 3 focused */
	int width;  /* -1: from the image */
	int height; /* -1: from the image */
	char hotkey;
	int focus_anim; /* anim played while focused, -1 for none */
	int focus_frame;
} button_t;

/* A clickable area, e.g. a save slot row (areabutton_t). */
typedef struct {
	int width;  /* -1: from the image */
	int height; /* -1: from the image */
	int image;  /* anim drawn when the form opens, -1 for none */
	int field_0c;
	char hotkey;
} areabutton_t;

/* A text entry field (edit_box_t). Colours are COLORREFs; create_edit_box sets them. */
typedef struct {
	int width, height;
	int max_chars;
	int lines;           /* 1: single line */
	const char *text;    /* initial text */
	unsigned background, foreground;
	unsigned focus_background, focus_foreground;
} edit_box_t;

/* Screen position of a save slot row. */
typedef struct {
	int x, y;
} mst_slot_t;
typedef mst_slot_t mns_slot_t;

/* A key that can be assigned, with its name (key_list). */
typedef struct {
	int vk; /* Windows virtual-key code */
	const char *name;
} mc_keyset_t;

/* One key setting on the controls screen (config_key). */
typedef struct {
	int x, y, x2, y2;
	const char *label;
	int key;
} mc_config_key_t;

/* One high-score entry (SCORE.A2R), laid out as in the file. */
#pragma pack(push, 1)
typedef struct {
	char name[32];
	int time;       /* seconds */
	short stage;    /* last stage reached (overall list) */
	short is_new;   /* 1 for the entry added last */
	int money;
} single_score_t;
#pragma pack(pop)

/* Help text for a control; a table ends with control_id -1. */
typedef struct {
	int control_id;
	const char *text;
} help_t;

#endif
