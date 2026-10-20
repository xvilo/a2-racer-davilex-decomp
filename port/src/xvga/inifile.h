/* INI files (inifile.cpp). The original wrapped GetPrivateProfileInt and
 * WritePrivateProfileString on a file in the game directory; the port reads and writes
 * the same "[SECTION]" / "KEY=value" text itself, in the save directory. */
#ifndef A2_INIFILE_H
#define A2_INIFILE_H

void set_ini_filename(const char *name);
void write_ini_string_setting(const char *section, const char *key, const char *value);
void write_ini_value_setting(const char *section, const char *key, int value);
/* 0 when the file, the section or the key is missing (GetPrivateProfileInt's default). */
unsigned read_ini_value_setting(const char *section, const char *key);
void read_ini_string_setting(const char *section, const char *key, char *buf, int size);

#endif
