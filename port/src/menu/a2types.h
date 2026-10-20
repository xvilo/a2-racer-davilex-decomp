/* Structures saved to disk by the menu, laid out exactly as in A2MENU.DAT so the
 * port reads and writes the original's OPTIE.A2R, BEDIEN.A2R and SLOT.A2R. */
#ifndef A2_A2TYPES_H
#define A2_A2TYPES_H

#include <stdint.h>

#pragma pack(push, 1)

/* OPTIE.A2R, and each save slot's copy. */
typedef struct {
	int16_t view;
	int16_t resolution;
	int16_t sound;         /* SOUND_* */
	int16_t display_mode;  /* 0 3D accelerator, 1 320x200, 2 640x480 */
	int16_t horizon;
	int16_t dashboard;
	int16_t detail;
	int16_t camera;
	int16_t music_volume;  /* 0..4 */
	int16_t effects_volume; /* 0..4 */
} opties_t;

enum {
	SOUND_ALL = 0,          /* music and effects */
	SOUND_EFFECTS_ONLY = 1,
	SOUND_OFF = 2,
	SOUND_MUSIC_ONLY = 3,
};

/* BEDIEN.A2R: controls. Keys are Windows virtual-key codes. */
typedef struct {
	int32_t control_type; /* 0 keyboard, 1 joypad, 2 steering wheel */
	int32_t field_04;
	int32_t field_08;
	int32_t key_left;
	int32_t key_right;
	int32_t key_gas;
	int32_t key_brake;
	int32_t key_action1;
	int32_t key_action2;
} bedien_t;

#define SLOT_COUNT 10
#define STAGE_COUNT 6

/* SLOT.A2R holds SLOT_COUNT of these. */
typedef struct {
	uint8_t used;
	uint8_t save_order;   /* the highest is the most recently saved */
	opties_t opties;
	bedien_t control;
	char name[32];
	int16_t field_5a;
	int16_t field_5c;
	int16_t stage;        /* current stage, 0..5 */
	int32_t time[STAGE_COUNT];   /* the player's time per stage, seconds */
	int32_t money;        /* 1000 at the start */
	int16_t position[STAGE_COUNT]; /* 6: not raced */
	int32_t time_racer[5][STAGE_COUNT];
	int16_t racer[5][STAGE_COUNT];
} slot_t;

#pragma pack(pop)

typedef char opties_size_check[sizeof(opties_t) == 0x14 ? 1 : -1];
typedef char bedien_size_check[sizeof(bedien_t) == 0x24 ? 1 : -1];
typedef char slot_size_check[sizeof(slot_t) == 0x13c ? 1 : -1];

#endif
