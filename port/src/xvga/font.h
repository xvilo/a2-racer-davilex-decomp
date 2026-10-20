/* Text drawing (font.cpp). The original created Windows GDI fonts and drew with
 * ExtTextOut into the DirectDraw surfaces; the port renders the same TrueType font
 * (Arial Black) with SDL_ttf, unsmoothed, in the nearest palette colour. */
#ifndef A2_FONT_H
#define A2_FONT_H

#include <stdint.h>

#define MAX_FONTS 6

/* Where to look for font files besides the system font folders; may be NULL. */
void font_init(const char *font_dir);
void font_shutdown(void);

/* height: GDI cell height in pixels (CreateFont's positive nHeight). Returns 0, or
 * 10000 when the font cannot be created (as the original). */
int xvga_load_font(int index, const char *face, int height, int italic, int underline);
void xvga_select_font(int index);
/* COLORREF values: 0x00BBGGRR. */
void xvga_set_font_colors(int index, uint32_t background, uint32_t text);
/* 1 transparent, 2 opaque (GDI's TRANSPARENT/OPAQUE). */
void xvga_set_font_bkmode(int index, int mode);
void xvga_free_font(int index);
int xvga_get_active_font(void);

int get_string_width(const char *text, int length);
int get_string_height(const char *text, int length);
/* Prints formatted text with the selected font into screen `which` (0 front, 1 back,
 * 2 buffer), the top of the text cell at y. */
int win_print_xy(int which, int x, int y, const char *fmt, ...);

#endif
