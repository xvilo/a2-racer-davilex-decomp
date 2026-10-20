#include "menu/txttool.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "xvga/font.h"

void win_bprintf(int which, int x, int y, int max_width, const char *fmt, ...)
{
	char text[256];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(text, sizeof text, fmt, ap);
	va_end(ap);
	/* Trailing blanks are dropped. */
	for (int j = (int)strlen(text) - 1; j > 0 && strchr(" \n\t", text[j]); j--)
		text[j] = 0;
	for (;;) {
		int n = (int)strlen(text);
		int w = get_string_width(text, n);
		if (w < max_width || w < 5 || n < 4) {
			win_print_xy(which, x, y, "%s", text);
			return;
		}
		/* One character shorter, ending in "...". */
		text[n - 1] = 0;
		text[n - 2] = '.';
		text[n - 3] = '.';
		text[n - 4] = '.';
	}
}

static int is_blank(unsigned char c)
{
	return c == 9 || c == 10 || c == ' ';
}

/* txt_get_word: blanks before a word plus the word; returns the characters used. */
static int txt_get_word(const char *s, char *word, int size)
{
	int i = 0, seen = 0;
	for (; s[i]; i++) {
		if (is_blank((unsigned char)s[i])) {
			if (seen)
				break;
		}
		else {
			seen = 1;
		}
		if (i + 1 < size)
			word[i] = s[i];
	}
	word[i < size ? i : size - 1] = 0;
	return i;
}

/* txt_check_newline: whether the blanks before the word contain a line break. */
static int txt_check_newline(const char *word)
{
	for (; *word && is_blank((unsigned char)*word); word++) {
		if (*word == 10 || *word == 9)
			return 1;
	}
	return 0;
}

static const char *txt_crop_word(const char *word)
{
	while (*word && is_blank((unsigned char)*word))
		word++;
	return word;
}

void win_printwrapped(int which, int x, int y, int width, const char *fmt, ...)
{
	char text[256], word[64];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(text, sizeof text, fmt, ap);
	va_end(ap);
	int right = x + width, len = (int)strlen(text), pos = 0, cx = x;
	while (pos < len) {
		pos += txt_get_word(text + pos, word, sizeof word);
		const char *out = word;
		int w = get_string_width(word, (int)strlen(word));
		if (w + cx >= right || txt_check_newline(word)) {
			/* Next line, without the blanks in front of the word. */
			y += get_string_height("H", 1);
			out = txt_crop_word(word);
			w = get_string_width(out, (int)strlen(out));
			cx = x;
		}
		win_print_xy(which, cx, y, "%s", out);
		cx += w;
	}
}
