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
