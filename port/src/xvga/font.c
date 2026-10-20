#include "xvga/font.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>
#include <SDL_ttf.h>

#include "gfx/screen.h"

typedef struct {
	TTF_Font *font;
	uint32_t background, text; /* COLORREF */
	int bkmode;
} xvga_font;

static xvga_font fonts[MAX_FONTS];
static int active_font = -1;
static char extra_dir[1024];

void font_init(const char *font_dir)
{
	if (font_dir)
		snprintf(extra_dir, sizeof extra_dir, "%s", font_dir);
	TTF_Init();
}

void font_shutdown(void)
{
	for (int i = 0; i < MAX_FONTS; i++)
		xvga_free_font(i);
	TTF_Quit();
}

/* Font files for a face name: the Windows file names, and those used by the
 * Debian/Ubuntu msttcorefonts package and macOS. */
static const char *const *files_for(const char *face)
{
	static const char *const arial_black[] = { "ariblk.ttf", "ARIBLK.TTF", "Arial_Black.ttf",
		"Arial Black.ttf", NULL };
	static const char *const arial[] = { "arial.ttf", "ARIAL.TTF", "Arial.ttf", NULL };
	if (!strcmp(face, "Arial Black"))
		return arial_black;
	return arial;
}

static TTF_Font *open_file(const char *path, int size)
{
	FILE *f = fopen(path, "rb");
	if (!f)
		return NULL;
	fclose(f);
	return TTF_OpenFont(path, size);
}

static TTF_Font *open_face(const char *face, int size)
{
	const char *dirs[8];
	int n = 0;
	char windir[512] = "";
	if (extra_dir[0])
		dirs[n++] = extra_dir;
	const char *w = getenv("WINDIR");
	if (w) {
		snprintf(windir, sizeof windir, "%s/Fonts", w);
		dirs[n++] = windir;
	}
	dirs[n++] = "/mnt/c/Windows/Fonts"; /* Linux under WSL */
	dirs[n++] = "/usr/share/fonts/truetype/msttcorefonts";
	dirs[n++] = "/System/Library/Fonts/Supplemental";
	dirs[n++] = "/Library/Fonts";
	for (int d = 0; d < n; d++) {
		for (const char *const *f = files_for(face); *f; f++) {
			char path[1200];
			snprintf(path, sizeof path, "%s/%s", dirs[d], *f);
			TTF_Font *font = open_file(path, size);
			if (font)
				return font;
		}
	}
	return NULL;
}

int xvga_load_font(int index, const char *face, int height, int italic, int underline)
{
	if (index < 0 || index >= MAX_FONTS || fonts[index].font)
		return 10000;
	/* GDI's positive height is the cell height (ascent + descent); find the point size
	 * that gives the same height. */
	TTF_Font *font = NULL;
	for (int size = height; size > 1; size--) {
		TTF_Font *f = open_face(face, size);
		if (!f) {
			fprintf(stderr, "font: cannot find \"%s\" (pass --fonts <dir> with ariblk.ttf)\n",
				face);
			return 10000;
		}
		if (TTF_FontHeight(f) <= height) {
			font = f;
			break;
		}
		TTF_CloseFont(f);
	}
	if (!font)
		return 10000;
	TTF_SetFontStyle(font, (italic ? TTF_STYLE_ITALIC : 0) | (underline ? TTF_STYLE_UNDERLINE : 0));
	TTF_SetFontHinting(font, TTF_HINTING_MONO);
	fonts[index].font = font;
	fonts[index].background = 0;
	fonts[index].text = 0xFFFFFF;
	fonts[index].bkmode = 2;
	return 0;
}

void xvga_select_font(int index)
{
	if (index >= 0 && index < MAX_FONTS)
		active_font = index;
}

void xvga_set_font_colors(int index, uint32_t background, uint32_t text)
{
	if (index >= 0 && index < MAX_FONTS) {
		fonts[index].background = background;
		fonts[index].text = text;
	}
}

void xvga_set_font_bkmode(int index, int mode)
{
	if (index >= 0 && index < MAX_FONTS)
		fonts[index].bkmode = mode;
}

int xvga_get_active_font(void)
{
	return active_font;
}

void xvga_free_font(int index)
{
	if (index >= 0 && index < MAX_FONTS && fonts[index].font) {
		TTF_CloseFont(fonts[index].font);
		fonts[index].font = NULL;
	}
}

int get_string_width(const char *text, int length)
{
	if (active_font < 0 || !fonts[active_font].font)
		return 0;
	char buf[512];
	snprintf(buf, sizeof buf, "%.*s", length, text);
	int w = 0, h = 0;
	TTF_SizeText(fonts[active_font].font, buf, &w, &h);
	return w;
}

int get_string_height(const char *text, int length)
{
	if (active_font < 0 || !fonts[active_font].font)
		return 0;
	return TTF_FontHeight(fonts[active_font].font);
}

/* Windows-1252 text from the game data to the UTF-8 SDL_ttf expects. */
static void cp1252_to_utf8(const char *in, char *out, size_t size)
{
	size_t o = 0;
	for (; *in && o + 3 < size; in++) {
		unsigned char c = (unsigned char)*in;
		if (c < 0x80) {
			out[o++] = (char)c;
		}
		else {
			out[o++] = (char)(0xC0 | c >> 6);
			out[o++] = (char)(0x80 | (c & 0x3F));
		}
	}
	out[o] = 0;
}

int win_print_xy(int which, int x, int y, const char *fmt, ...)
{
	char text[400], utf8[1200];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(text, sizeof text, fmt, ap);
	va_end(ap);
	if (active_font < 0 || !fonts[active_font].font || !text[0])
		return 0;
	xvga_font *f = &fonts[active_font];
	cp1252_to_utf8(text, utf8, sizeof utf8);
	SDL_Color white = { 255, 255, 255, 255 };
	SDL_Surface *s = TTF_RenderUTF8_Solid(f->font, utf8, white);
	if (!s)
		return 0;
	uint8_t fg = nearest_color(f->text), bg = nearest_color(f->background);
	SDL_LockSurface(s);
	for (int row = 0; row < s->h; row++) {
		int sy = y + row;
		if (sy < 0 || sy >= SCREEN_H)
			continue;
		const uint8_t *src = (const uint8_t *)s->pixels + row * s->pitch;
		uint8_t *dst = screen.pixels[which] + sy * SCREEN_W;
		for (int col = 0; col < s->w; col++) {
			int sx = x + col;
			if (sx < 0 || sx >= SCREEN_W)
				continue;
			if (src[col])
				dst[sx] = fg;
			else if (f->bkmode == 2)
				dst[sx] = bg;
		}
	}
	SDL_UnlockSurface(s);
	SDL_FreeSurface(s);
	return 0;
}
