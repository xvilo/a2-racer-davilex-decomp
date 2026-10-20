#include "xvga/inifile.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xvga/fileio.h"

static char ini_filename[1200];
static int ini_file_init_ok;

void set_ini_filename(const char *name)
{
	if (strlen(name) >= 0x10) {
		fprintf(stderr, "set_ini_filename: %s: file name too long\n", name);
		exit(1);
	}
	snprintf(ini_filename, sizeof ini_filename, "%s", save_path(name));
	ini_file_init_ok = 1;
}

void set_ini_file_path(const char *path)
{
	snprintf(ini_filename, sizeof ini_filename, "%s", path);
	ini_file_init_ok = 1;
}

static void check_init(const char *who)
{
	if (!ini_file_init_ok) {
		fprintf(stderr, "%s: no INI file set\n", who);
		exit(1);
	}
}

/* The whole file in memory, or NULL. */
static char *load_text(void)
{
	FILE *f = fopen(ini_filename, "rb");
	if (!f)
		return NULL;
	fseek(f, 0, SEEK_END);
	long n = ftell(f);
	fseek(f, 0, SEEK_SET);
	char *text = malloc((size_t)n + 1);
	if (text) {
		n = (long)fread(text, 1, (size_t)n, f);
		text[n] = 0;
	}
	fclose(f);
	return text;
}

static const char *skip_blank(const char *p)
{
	while (*p == ' ' || *p == '\t')
		p++;
	return p;
}

/* Compares `name` with the text from p up to `end`, ignoring case and trailing blanks. */
static int name_matches(const char *p, const char *end, const char *name)
{
	while (end > p && (end[-1] == ' ' || end[-1] == '\t'))
		end--;
	size_t len = strlen(name);
	if ((size_t)(end - p) != len)
		return 0;
	for (size_t i = 0; i < len; i++) {
		if (tolower((unsigned char)p[i]) != tolower((unsigned char)name[i]))
			return 0;
	}
	return 1;
}

/* Finds the key in the section. Returns its line's start and sets *value to the text
 * after '=' (up to the end of the line); with no such key, returns NULL and sets
 * *insert to where a new key goes (the end of the section), or NULL when the section
 * is missing. */
static const char *find_key(const char *text, const char *section, const char *key,
	const char **value, const char **insert)
{
	int in_section = 0;
	*insert = NULL;
	for (const char *line = text; *line;) {
		const char *eol = line + strcspn(line, "\r\n");
		const char *next = eol;
		while (*next == '\r' || *next == '\n')
			next++;
		const char *p = skip_blank(line);
		if (*p == '[') {
			const char *close = memchr(p, ']', (size_t)(eol - p));
			in_section = close && name_matches(p + 1, close, section);
		}
		else if (in_section && p < eol) {
			const char *eq = memchr(p, '=', (size_t)(eol - p));
			if (eq && name_matches(p, eq, key)) {
				*value = eq + 1;
				return line;
			}
		}
		if (in_section)
			*insert = next;
		line = next;
	}
	return NULL;
}

void write_ini_string_setting(const char *section, const char *key, const char *value)
{
	check_init("write_ini_string_setting");
	char *text = load_text();
	const char *old = text ? text : "";
	const char *v, *insert;
	const char *line = find_key(old, section, key, &v, &insert);
	FILE *f = fopen(ini_filename, "wb");
	if (!f) {
		free(text);
		return;
	}
	if (line) {
		/* Replace the value, keep the rest. */
		const char *eol = v + strcspn(v, "\r\n");
		fwrite(old, 1, (size_t)(line - old), f);
		fprintf(f, "%s=%s", key, value);
		fputs(eol, f);
	}
	else if (insert) {
		/* A new key at the end of the section; the line before it may lack its end. */
		fwrite(old, 1, (size_t)(insert - old), f);
		if (insert > old && insert[-1] != '\n')
			fputs("\r\n", f);
		fprintf(f, "%s=%s\r\n", key, value);
		fputs(insert, f);
	}
	else {
		fputs(old, f);
		size_t n = strlen(old);
		if (n && old[n - 1] != '\n')
			fputs("\r\n", f);
		fprintf(f, "[%s]\r\n%s=%s\r\n", section, key, value);
	}
	fclose(f);
	free(text);
}

void write_ini_value_setting(const char *section, const char *key, int value)
{
	char buf[36];
	check_init("write_ini_value_setting");
	snprintf(buf, sizeof buf, "%d", value);
	write_ini_string_setting(section, key, buf);
}

void read_ini_string_setting(const char *section, const char *key, char *buf, int size)
{
	check_init("read_ini_string_setting");
	char *text = load_text();
	const char *v = NULL, *insert;
	buf[0] = 0;
	if (text && find_key(text, section, key, &v, &insert)) {
		v = skip_blank(v);
		int n = (int)strcspn(v, "\r\n");
		while (n > 0 && (v[n - 1] == ' ' || v[n - 1] == '\t'))
			n--;
		snprintf(buf, (size_t)size, "%.*s", n, v);
	}
	free(text);
}

unsigned read_ini_value_setting(const char *section, const char *key)
{
	char buf[64];
	check_init("read_ini_value_setting");
	read_ini_string_setting(section, key, buf, sizeof buf);
	return (unsigned)strtol(buf, NULL, 10);
}
