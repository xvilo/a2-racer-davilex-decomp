#include "menu/infio.h"

#include <string.h>

#include "menu/menu_data.h"
#include "xvga/fileio.h"
#include "xvga/inifile.h"

infio_t infio;

void load_infio(void)
{
	if (!file_exist("INFIO.INI")) {
		file_delete("INFIO.INI");
		init_infio();
		return;
	}
	set_ini_filename("INFIO.INI");
	infio.race_finished = (uint8_t)read_ini_value_setting("GAME", "RACE_KLAAR");
	infio.time = (int32_t)read_ini_value_setting("GAME", "TIJD");
	infio.active_slot = (int16_t)read_ini_value_setting("GAME", "ACTIVE_SLOT");
	infio.money = (int32_t)read_ini_value_setting("GAME", "GELD");
	infio.stage = (int16_t)read_ini_value_setting("GAME", "ETAPPE");
	infio.ended_by = (int16_t)read_ini_value_setting("GAME", "EINDE_DOOR");
	infio.position = (int16_t)read_ini_value_setting("GAME", "POSITIE");
	infio.racer_1 = (int16_t)read_ini_value_setting("GAME", "RACER_1");
	infio.racer_2 = (int16_t)read_ini_value_setting("GAME", "RACER_2");
	infio.racer_3 = (int16_t)read_ini_value_setting("GAME", "RACER_3");
	infio.racer_4 = (int16_t)read_ini_value_setting("GAME", "RACER_4");
	infio.racer_5 = (int16_t)read_ini_value_setting("GAME", "RACER_5");
	/* The rivals' times go through a short, as in the original. */
	infio.time_racer_1 = (int16_t)read_ini_value_setting("GAME", "TIJD_1");
	infio.time_racer_2 = (int16_t)read_ini_value_setting("GAME", "TIJD_2");
	infio.time_racer_3 = (int16_t)read_ini_value_setting("GAME", "TIJD_3");
	infio.time_racer_4 = (int16_t)read_ini_value_setting("GAME", "TIJD_4");
	infio.time_racer_5 = (int16_t)read_ini_value_setting("GAME", "TIJD_5");
	infio.final = (int16_t)read_ini_value_setting("GAME", "FINALE");
	infio.control_type = (int16_t)read_ini_value_setting("CONTROL", "BEDIENING");
	infio.key_left = (int16_t)read_ini_value_setting("CONTROL", "KEY_LINKS");
	infio.key_right = (int16_t)read_ini_value_setting("CONTROL", "KEY_RECHTS");
	infio.key_gas = (int16_t)read_ini_value_setting("CONTROL", "KEY_GAS");
	infio.key_brake = (int16_t)read_ini_value_setting("CONTROL", "KEY_REM");
	infio.key_action1 = (int16_t)read_ini_value_setting("CONTROL", "KEY_ACTIE1");
	infio.key_action2 = (int16_t)read_ini_value_setting("CONTROL", "KEY_ACTIE2");
	infio.sound = (int16_t)read_ini_value_setting("OPTION", "GELUID");
	infio.resolution = (int16_t)read_ini_value_setting("OPTION", "RESOLUTIE");
	infio.view = (int16_t)read_ini_value_setting("OPTION", "VIEW");
	infio.display_mode = (int16_t)read_ini_value_setting("OPTION", "DISPLAY_MODE");
	infio.horizon = (int16_t)read_ini_value_setting("OPTION", "HORIZON");
	infio.dashboard = (int16_t)read_ini_value_setting("OPTION", "DASHBORD");
	infio.detail = (int16_t)read_ini_value_setting("OPTION", "DETAIL");
	infio.camera = (int16_t)read_ini_value_setting("OPTION", "CAMERA");
	infio.music_volume = (int16_t)read_ini_value_setting("OPTION", "VMUZIEK");
	infio.effects_volume = (int16_t)read_ini_value_setting("OPTION", "VEFFECT");
}

void save_infio(void)
{
	set_ini_filename("INFIO.INI");
	write_ini_value_setting("GAME", "RACE_KLAAR", infio.race_finished);
	write_ini_value_setting("GAME", "TIJD", infio.time);
	write_ini_value_setting("GAME", "ACTIVE_SLOT", infio.active_slot);
	write_ini_value_setting("GAME", "GELD", infio.money);
	write_ini_value_setting("GAME", "ETAPPE", infio.stage);
	write_ini_value_setting("GAME", "EINDE_DOOR", infio.ended_by);
	write_ini_value_setting("GAME", "POSITIE", infio.position);
	write_ini_value_setting("GAME", "RACER_1", infio.racer_1);
	write_ini_value_setting("GAME", "RACER_2", infio.racer_2);
	write_ini_value_setting("GAME", "RACER_3", infio.racer_3);
	write_ini_value_setting("GAME", "RACER_4", infio.racer_4);
	write_ini_value_setting("GAME", "RACER_5", infio.racer_5);
	write_ini_value_setting("GAME", "TIJD_1", infio.time_racer_1);
	write_ini_value_setting("GAME", "TIJD_2", infio.time_racer_2);
	write_ini_value_setting("GAME", "TIJD_3", infio.time_racer_3);
	write_ini_value_setting("GAME", "TIJD_4", infio.time_racer_4);
	write_ini_value_setting("GAME", "TIJD_5", infio.time_racer_5);
	write_ini_value_setting("GAME", "FINALE", infio.final);
	write_ini_value_setting("CONTROL", "BEDIENING", infio.control_type);
	write_ini_value_setting("CONTROL", "KEY_LINKS", infio.key_left);
	write_ini_value_setting("CONTROL", "KEY_RECHTS", infio.key_right);
	write_ini_value_setting("CONTROL", "KEY_GAS", infio.key_gas);
	write_ini_value_setting("CONTROL", "KEY_REM", infio.key_brake);
	write_ini_value_setting("CONTROL", "KEY_ACTIE1", infio.key_action1);
	write_ini_value_setting("CONTROL", "KEY_ACTIE2", infio.key_action2);
	write_ini_value_setting("OPTION", "GELUID", infio.sound);
	write_ini_value_setting("OPTION", "RESOLUTIE", infio.resolution);
	write_ini_value_setting("OPTION", "VIEW", infio.view);
	write_ini_value_setting("OPTION", "DISPLAY_MODE", infio.display_mode);
	write_ini_value_setting("OPTION", "HORIZON", infio.horizon);
	write_ini_value_setting("OPTION", "DASHBORD", infio.dashboard);
	write_ini_value_setting("OPTION", "DETAIL", infio.detail);
	write_ini_value_setting("OPTION", "CAMERA", infio.camera);
	write_ini_value_setting("OPTION", "VMUZIEK", infio.music_volume);
	write_ini_value_setting("OPTION", "VEFFECT", infio.effects_volume);
}

void init_infio(void)
{
	if (file_exist("INFIO.INI")) {
		load_infio();
		return;
	}
	memset(&infio, 0, sizeof infio);
	infio.racer_1 = 1;
	infio.racer_2 = 2;
	infio.racer_3 = 3;
	infio.racer_4 = 4;
	infio.racer_5 = 5;
	infio.music_volume = 3;
	infio.effects_volume = 4;
	infio.display_mode = 1;
	infio.detail = 1;
	infio.position = 6;
	/* The original left money at whatever EDX held; reset_infio sets it before a race. */
	infio.money = 0;
	int16_t *keys = &infio.key_left;
	for (int i = 0; i < 6; i++)
		keys[i] = toets_define[i];
	save_infio();
}
