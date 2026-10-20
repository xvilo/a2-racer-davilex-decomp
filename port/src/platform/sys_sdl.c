#include "platform/sys.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#include "gfx/screen.h"

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static uint32_t rgba[SCREEN_W * SCREEN_H];
static bool headless;
/* Mouse state survives across sys_input structs: the game polls from many nested loops. */
static int mouse_x, mouse_y;
static bool mouse_down;

/* Scripted input for tests: one command per line, run in order as time passes.
 *   wait <ms>         pause the script
 *   key <name>        press and release a key (enter, escape, up, down, left, right,
 *                     tab, space, backspace, or a letter/digit; alt+<x> holds Alt)
 *   text <string>     type text
 *   click <x> <y>     left click at a 640x480 position
 *   shot <file.png>   save the front screen
 *   quit              close the window
 */
static char **script_lines;
static int script_count, script_pos;
static uint32_t script_resume;
static sys_screenshot_fn screenshot_fn;
static bool fake_alt;
static SDL_Scancode fake_key = SDL_SCANCODE_UNKNOWN; /* held for keyispressed */
static uint32_t fake_key_until;

void sys_set_screenshot(sys_screenshot_fn fn)
{
	screenshot_fn = fn;
}

static void load_script(const char *path)
{
	FILE *f = fopen(path, "r");
	char line[512];
	if (!f) {
		SDL_Log("script: cannot open %s", path);
		return;
	}
	while (fgets(line, sizeof line, f)) {
		line[strcspn(line, "\r\n")] = 0;
		if (!line[0] || line[0] == '#')
			continue;
		script_lines = realloc(script_lines, sizeof *script_lines * (size_t)(script_count + 1));
		script_lines[script_count++] = SDL_strdup(line);
	}
	fclose(f);
}

static SDL_Keycode key_by_name(const char *n)
{
	static const struct { const char *name; SDL_Keycode key; } keys[] = {
		{ "enter", SDLK_RETURN }, { "escape", SDLK_ESCAPE }, { "up", SDLK_UP },
		{ "down", SDLK_DOWN }, { "left", SDLK_LEFT }, { "right", SDLK_RIGHT },
		{ "tab", SDLK_TAB }, { "space", SDLK_SPACE }, { "backspace", SDLK_BACKSPACE },
	};
	for (size_t i = 0; i < sizeof keys / sizeof keys[0]; i++) {
		if (!strcmp(n, keys[i].name))
			return keys[i].key;
	}
	return (SDL_Keycode)(unsigned char)n[0];
}

static void push_key(SDL_Keycode k)
{
	SDL_Event e = { 0 };
	e.type = SDL_KEYDOWN;
	e.key.keysym.sym = k;
	e.key.keysym.scancode = SDL_GetScancodeFromKey(k);
	fake_key = e.key.keysym.scancode;
	fake_key_until = SDL_GetTicks() + 100;
	SDL_PushEvent(&e);
	e.type = SDL_KEYUP;
	SDL_PushEvent(&e);
}

static void run_script(void)
{
	while (script_pos < script_count && SDL_GetTicks() >= script_resume) {
		char *cmd = script_lines[script_pos++];
		SDL_Event e = { 0 };
		fake_alt = false;
		if (!strncmp(cmd, "wait ", 5)) {
			script_resume = SDL_GetTicks() + (uint32_t)atoi(cmd + 5);
		}
		else if (!strncmp(cmd, "key ", 4)) {
			const char *k = cmd + 4;
			fake_alt = !strncmp(k, "alt+", 4);
			push_key(key_by_name(fake_alt ? k + 4 : k));
			script_resume = SDL_GetTicks() + 50;
			break;
		}
		else if (!strncmp(cmd, "text ", 5)) {
			e.type = SDL_TEXTINPUT;
			SDL_strlcpy(e.text.text, cmd + 5, sizeof e.text.text);
			SDL_PushEvent(&e);
		}
		else if (!strncmp(cmd, "click ", 6)) {
			int x = 0, y = 0;
			sscanf(cmd + 6, "%d %d", &x, &y);
			e.type = SDL_MOUSEBUTTONDOWN;
			e.button.button = SDL_BUTTON_LEFT;
			e.button.x = x;
			e.button.y = y;
			SDL_PushEvent(&e);
			e.type = SDL_MOUSEBUTTONUP;
			SDL_PushEvent(&e);
			script_resume = SDL_GetTicks() + 50;
			break;
		}
		else if (!strncmp(cmd, "shot ", 5)) {
			if (screenshot_fn)
				screenshot_fn(cmd + 5);
		}
		else if (!strcmp(cmd, "quit")) {
			e.type = SDL_QUIT;
			SDL_PushEvent(&e);
		}
	}
}

bool sys_init(const char *title, const sys_options *opt)
{
	headless = opt->hidden;
	if (opt->script)
		load_script(opt->script);
	if (headless)
		return SDL_Init(SDL_INIT_TIMER) == 0;
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
		SDL_Log("SDL_Init: %s", SDL_GetError());
		return false;
	}
	int scale = opt->scale > 0 ? opt->scale : 1;
	window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		SCREEN_W * scale, SCREEN_H * scale, SDL_WINDOW_RESIZABLE);
	if (!window) {
		SDL_Log("SDL_CreateWindow: %s", SDL_GetError());
		return false;
	}
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);
	if (!renderer)
		renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
	if (!renderer) {
		SDL_Log("SDL_CreateRenderer: %s", SDL_GetError());
		return false;
	}
	/* Keep the 4:3 picture and map mouse coordinates back to 640x480. */
	SDL_RenderSetLogicalSize(renderer, SCREEN_W, SCREEN_H);
	texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
		SDL_TEXTUREACCESS_STREAMING, SCREEN_W, SCREEN_H);
	SDL_StopTextInput(); /* only while an edit box is open */
	return texture != NULL;
}

void sys_shutdown(void)
{
	if (texture)
		SDL_DestroyTexture(texture);
	if (renderer)
		SDL_DestroyRenderer(renderer);
	if (window)
		SDL_DestroyWindow(window);
	SDL_Quit();
}

void sys_present(const uint8_t *pixels, const uint8_t palette[768])
{
	if (headless)
		return;
	uint32_t lut[256];
	for (int i = 0; i < 256; i++)
		lut[i] = 0xFF000000u | palette[i * 3] << 16 | palette[i * 3 + 1] << 8 | palette[i * 3 + 2];
	for (int i = 0; i < SCREEN_W * SCREEN_H; i++)
		rgba[i] = lut[pixels[i]];
	SDL_UpdateTexture(texture, NULL, rgba, SCREEN_W * 4);
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);
}

static int map_key(SDL_Keycode k)
{
	switch (k) {
	case SDLK_ESCAPE: return KEY_ESCAPE;
	case SDLK_RETURN:
	case SDLK_KP_ENTER: return KEY_ENTER;
	case SDLK_UP: return KEY_UP;
	case SDLK_DOWN: return KEY_DOWN;
	case SDLK_LEFT: return KEY_LEFT;
	case SDLK_RIGHT: return KEY_RIGHT;
	default: return k > 0 && k < 128 ? (int)k : KEY_NONE;
	}
}

void sys_poll(sys_input *in)
{
	in->mouse_clicked = false;
	in->mouse_pressed = false;
	in->key = KEY_NONE;
	in->text[0] = 0;
	in->backspaces = 0;
	if (headless)
		return;
	run_script();
	SDL_Event e;
	while (SDL_PollEvent(&e)) {
		switch (e.type) {
		case SDL_QUIT:
			in->quit = true;
			break;
		case SDL_MOUSEMOTION:
			mouse_x = e.motion.x;
			mouse_y = e.motion.y;
			break;
		case SDL_MOUSEBUTTONDOWN:
			if (e.button.button == SDL_BUTTON_LEFT) {
				mouse_x = e.button.x;
				mouse_y = e.button.y;
				mouse_down = true;
				in->mouse_pressed = true;
			}
			break;
		case SDL_MOUSEBUTTONUP:
			if (e.button.button == SDL_BUTTON_LEFT) {
				mouse_x = e.button.x;
				mouse_y = e.button.y;
				mouse_down = false;
				in->mouse_clicked = true;
			}
			break;
		case SDL_KEYDOWN:
			in->key = map_key(e.key.keysym.sym);
			if (e.key.keysym.sym == SDLK_BACKSPACE)
				in->backspaces++;
			break;
		case SDL_TEXTINPUT: {
			/* UTF-8 to Windows-1252 (Latin-1 range), as the game's text is. */
			size_t n = strlen(in->text);
			for (const unsigned char *p = (const unsigned char *)e.text.text; *p && n + 1 < sizeof in->text;
				p++) {
				if (*p < 0x80)
					in->text[n++] = (char)*p;
				else if ((*p & 0xE0) == 0xC0 && p[1]) {
					in->text[n++] = (char)(((*p & 0x1F) << 6) | (p[1] & 0x3F));
					p++;
				}
			}
			in->text[n] = 0;
			break;
		}
		}
	}
	in->mouse_x = mouse_x;
	in->mouse_y = mouse_y;
	in->mouse_down = mouse_down;
}

/* Windows virtual-key codes to SDL scancodes, for the keys the game asks about. */
static SDL_Scancode vk_to_scancode(int vk)
{
	if (vk >= 'A' && vk <= 'Z')
		return (SDL_Scancode)(SDL_SCANCODE_A + vk - 'A');
	if (vk >= '1' && vk <= '9')
		return (SDL_Scancode)(SDL_SCANCODE_1 + vk - '1');
	switch (vk) {
	case '0': return SDL_SCANCODE_0;
	case 0x08: return SDL_SCANCODE_BACKSPACE;
	case 0x09: return SDL_SCANCODE_TAB;
	case 0x0d: return SDL_SCANCODE_RETURN;
	case 0x10: return SDL_SCANCODE_LSHIFT;
	case 0x11: return SDL_SCANCODE_LCTRL;
	case 0x12: return SDL_SCANCODE_LALT;
	case 0x1b: return SDL_SCANCODE_ESCAPE;
	case 0x20: return SDL_SCANCODE_SPACE;
	case 0x25: return SDL_SCANCODE_LEFT;
	case 0x26: return SDL_SCANCODE_UP;
	case 0x27: return SDL_SCANCODE_RIGHT;
	case 0x28: return SDL_SCANCODE_DOWN;
	default: return SDL_SCANCODE_UNKNOWN;
	}
}

bool keyispressed(int vk)
{
	if (headless)
		return false;
	if (vk == 0x12 && fake_alt)
		return true;
	SDL_PumpEvents();
	const Uint8 *state = SDL_GetKeyboardState(NULL);
	SDL_Scancode sc = vk_to_scancode(vk);
	if (sc == SDL_SCANCODE_UNKNOWN)
		return false;
	if (sc == fake_key && SDL_GetTicks() < fake_key_until)
		return true;
	if (vk == 0x10)
		return state[SDL_SCANCODE_LSHIFT] || state[SDL_SCANCODE_RSHIFT];
	if (vk == 0x11)
		return state[SDL_SCANCODE_LCTRL] || state[SDL_SCANCODE_RCTRL];
	if (vk == 0x12)
		return state[SDL_SCANCODE_LALT] || state[SDL_SCANCODE_RALT];
	if (vk == 0x0d)
		return state[SDL_SCANCODE_RETURN] || state[SDL_SCANCODE_KP_ENTER];
	return state[sc] != 0;
}

void sys_text_input(bool on)
{
	if (headless)
		return;
	if (on)
		SDL_StartTextInput();
	else
		SDL_StopTextInput();
}

uint32_t sys_ticks(void)
{
	return SDL_GetTicks();
}

void sys_sleep(uint32_t ms)
{
	SDL_Delay(ms);
}
