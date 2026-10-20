/* INFIO.INI (infio.cpp): what the menu and the race tell each other. The menu writes
 * the player's options, controls and standing before a race; the race writes back
 * whether it was finished, the times and the positions. */
#ifndef A2_INFIO_H
#define A2_INFIO_H

#include <stdint.h>

/* infio_t in A2MENU.DAT; the INI keys are in the comments. */
typedef struct {
	uint8_t race_finished; /* GAME/RACE_KLAAR */
	int32_t time;          /* GAME/TIJD, seconds */
	int32_t time_racer_1;  /* GAME/TIJD_1 .. TIJD_5 */
	int32_t time_racer_2;
	int32_t time_racer_3;
	int32_t time_racer_4;
	int32_t time_racer_5;
	int16_t active_slot;   /* GAME/ACTIVE_SLOT */
	int16_t sound;         /* OPTION/GELUID */
	int16_t resolution;    /* OPTION/RESOLUTIE */
	int16_t view;          /* OPTION/VIEW */
	int16_t control_type;  /* CONTROL/BEDIENING */
	int16_t key_left;      /* CONTROL/KEY_LINKS */
	int16_t key_right;     /* CONTROL/KEY_RECHTS */
	int16_t key_gas;       /* CONTROL/KEY_GAS */
	int16_t key_brake;     /* CONTROL/KEY_REM */
	int16_t key_action1;   /* CONTROL/KEY_ACTIE1 */
	int16_t key_action2;   /* CONTROL/KEY_ACTIE2 */
	int16_t display_mode;  /* OPTION/DISPLAY_MODE */
	int16_t horizon;       /* OPTION/HORIZON */
	int16_t dashboard;     /* OPTION/DASHBORD */
	int16_t detail;        /* OPTION/DETAIL */
	int16_t stage;         /* GAME/ETAPPE */
	int16_t camera;        /* OPTION/CAMERA */
	int16_t ended_by;      /* GAME/EINDE_DOOR: 0 quit, 1 fuel, 2 money, 3 time */
	int16_t position;      /* GAME/POSITIE */
	int16_t racer_1;       /* GAME/RACER_1 .. RACER_5: the rivals' positions */
	int16_t racer_2;
	int16_t racer_3;
	int16_t racer_4;
	int16_t racer_5;
	int16_t final;         /* GAME/FINALE */
	int16_t music_volume;  /* OPTION/VMUZIEK */
	int16_t effects_volume; /* OPTION/VEFFECT */
	int32_t money;         /* GAME/GELD */
} infio_t;

extern infio_t infio;

void load_infio(void);
void save_infio(void);
void init_infio(void);

#endif
