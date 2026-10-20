#include "platform/sys.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#include "gfx/png.h"
#include "gfx/screen.h"
#include "platform/gl.h"

static SDL_Window *window;
/* OpenGL when available (needed by the race); else an SDL renderer for the menus. */
static SDL_GLContext gl_context;
static GLuint screen_texture;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static uint32_t rgba[SCREEN_W * SCREEN_H];
static bool headless;
/* Mouse state survives across sys_input structs: the game polls from many nested loops. */
static int mouse_x, mouse_y;
static bool mouse_down;
/* A quit request stays set: menus nested several deep must all see it. */
static bool quit_requested;

/* Scripted input for tests: one command per line, run in order as time passes.
 *   wait <ms>         pause the script
 *   hold <name> <ms>  hold a key down for a while (for keyispressed)
 *   key <name>        press and release a key (enter, escape, up, down, left, right,
 *                     tab, space, backspace, f1..f12, or a letter/digit; alt+<x> holds Alt)
 *   text <string>     type text
 *   click <x> <y>     left click at a 640x480 position
 *   shot <file.png>   save the front screen
 *   glshot <file.png> save the window's picture as drawn (OpenGL only)
 *   quit              close the window
 */
static char **script_lines;
static int script_count, script_pos;
static uint32_t script_resume;
static sys_screenshot_fn screenshot_fn;
static bool fake_alt;
static SDL_Scancode fake_key = SDL_SCANCODE_UNKNOWN; /* held for keyispressed */
static char glshot_path[512];
static SDL_Scancode held_key[4];
static uint32_t held_until[4];
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
	if (n[0] == 'f' && n[1] >= '1' && n[1] <= '9')
		return SDLK_F1 + atoi(n + 1) - 1;
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
		else if (!strncmp(cmd, "hold ", 5)) {
			char name[32] = "";
			int ms = 0;
			sscanf(cmd + 5, "%31s %d", name, &ms);
			for (int i = 0; i < 4; i++) {
				if (held_until[i] <= SDL_GetTicks()) {
					held_key[i] = SDL_GetScancodeFromKey(key_by_name(name));
					held_until[i] = SDL_GetTicks() + (uint32_t)ms;
					break;
				}
			}
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
		else if (!strncmp(cmd, "glshot ", 7) && gl_context) {
			/* Taken from the next frame, before it is shown. */
			SDL_strlcpy(glshot_path, cmd + 7, sizeof glshot_path);
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
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		SCREEN_W * scale, SCREEN_H * scale, SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
	if (window) {
		gl_context = SDL_GL_CreateContext(window);
		if (gl_context && !gl_load()) {
			SDL_GL_DeleteContext(gl_context);
			gl_context = NULL;
		}
		if (!gl_context) {
			SDL_Log("no OpenGL (%s); the race needs it, the menus work without", SDL_GetError());
			SDL_DestroyWindow(window);
			window = NULL;
		}
	}
	if (gl_context) {
		SDL_GL_SetSwapInterval(1);
		glGenTextures(1, &screen_texture);
		glBindTexture(GL_TEXTURE_2D, screen_texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SCREEN_W, SCREEN_H, 0, GL_RGBA, GL_UNSIGNED_BYTE,
			NULL);
		SDL_StopTextInput(); /* only while an edit box is open */
		return true;
	}

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
	if (gl_context) {
		glDeleteTextures(1, &screen_texture);
		SDL_GL_DeleteContext(gl_context);
	}
	if (texture)
		SDL_DestroyTexture(texture);
	if (renderer)
		SDL_DestroyRenderer(renderer);
	if (window)
		SDL_DestroyWindow(window);
	SDL_Quit();
}

/* The 4:3 picture's place in the window (letterboxed), in pixels of `w` x `h`. */
static SDL_Rect fit_4_3(int w, int h)
{
	SDL_Rect r;
	if (w * SCREEN_H > h * SCREEN_W) {
		r.h = h;
		r.w = h * SCREEN_W / SCREEN_H;
	}
	else {
		r.w = w;
		r.h = w * SCREEN_H / SCREEN_W;
	}
	r.x = (w - r.w) / 2;
	r.y = (h - r.h) / 2;
	return r;
}

static SDL_Rect gl_picture_rect(void)
{
	int w, h;
	SDL_GL_GetDrawableSize(window, &w, &h);
	return fit_4_3(w, h);
}

/* Window coordinates to 640x480. */
static void map_mouse(int *x, int *y)
{
	if (!gl_context)
		return; /* SDL_RenderSetLogicalSize already did it */
	int w, h;
	SDL_GetWindowSize(window, &w, &h);
	SDL_Rect r = fit_4_3(w, h);
	*x = r.w ? (*x - r.x) * SCREEN_W / r.w : 0;
	*y = r.h ? (*y - r.y) * SCREEN_H / r.h : 0;
}

/* The indexed screen as RGBA; palette index `transparent` (or none for -1) clear. */
static void screen_to_rgba(const uint8_t *pixels, const uint8_t palette[768], int transparent)
{
	uint32_t lut[256];
	for (int i = 0; i < 256; i++) {
		const uint8_t *c = &palette[i * 3];
		/* RGBA bytes in memory order, for GL_RGBA/GL_UNSIGNED_BYTE and the SDL texture. */
		uint32_t a = i == transparent ? 0 : 0xFF;
		uint8_t px[4] = { c[0], c[1], c[2], (uint8_t)a };
		memcpy(&lut[i], px, 4);
	}
	for (int i = 0; i < SCREEN_W * SCREEN_H; i++)
		rgba[i] = lut[pixels[i]];
}

static void gl_swap(void)
{
	if (glshot_path[0]) {
		int w, h;
		sys_gl_read_picture(NULL, &w, &h);
		uint8_t *rgb = malloc((size_t)w * h * 3);
		if (rgb) {
			sys_gl_read_picture(rgb, &w, &h);
			png_write_rgb(glshot_path, rgb, w, h);
			free(rgb);
		}
		glshot_path[0] = 0;
	}
	SDL_GL_SwapWindow(window);
}

/* Draws the screen texture over the whole picture area. */
static void gl_draw_screen(bool blend)
{
	SDL_Rect r = gl_picture_rect();
	glViewport(r.x, r.y, r.w, r.h);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_FOG);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, SCREEN_W, SCREEN_H, 0, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, screen_texture);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SCREEN_W, SCREEN_H, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
	if (blend) {
		glEnable(GL_ALPHA_TEST);
		glAlphaFunc(GL_GREATER, 0.5f);
	}
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0);
	glVertex2f(0, 0);
	glTexCoord2f(1, 0);
	glVertex2f(SCREEN_W, 0);
	glTexCoord2f(1, 1);
	glVertex2f(SCREEN_W, SCREEN_H);
	glTexCoord2f(0, 1);
	glVertex2f(0, SCREEN_H);
	glEnd();
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_TEXTURE_2D);
}

void sys_present(const uint8_t *pixels, const uint8_t palette[768])
{
	if (headless)
		return;
	if (gl_context) {
		int w, h;
		SDL_GL_GetDrawableSize(window, &w, &h);
		glViewport(0, 0, w, h);
		glClearColor(0, 0, 0, 1);
		glClear(GL_COLOR_BUFFER_BIT);
		screen_to_rgba(pixels, palette, -1);
		gl_draw_screen(false);
		gl_swap();
		return;
	}
	screen_to_rgba(pixels, palette, -1);
	SDL_UpdateTexture(texture, NULL, rgba, SCREEN_W * 4);
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);
}

bool sys_gl(void)
{
	return gl_context != NULL;
}

void sys_gl_begin_frame(void)
{
	int w, h;
	SDL_GL_GetDrawableSize(window, &w, &h);
	glViewport(0, 0, w, h);
	glDisable(GL_SCISSOR_TEST);
	glClearColor(0, 0, 0, 1);
	glDepthMask(GL_TRUE);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void sys_gl_viewport(int x, int y, int w, int h)
{
	SDL_Rect r = gl_picture_rect();
	int gx = r.x + x * r.w / SCREEN_W, gw = w * r.w / SCREEN_W;
	int gh = h * r.h / SCREEN_H, gy = r.y + r.h - (y * r.h / SCREEN_H) - gh;
	glViewport(gx, gy, gw, gh);
	glScissor(gx, gy, gw, gh);
	glEnable(GL_SCISSOR_TEST);
}

void sys_gl_overlay(const uint8_t *pixels, const uint8_t palette[768], int transparent)
{
	glDisable(GL_SCISSOR_TEST);
	screen_to_rgba(pixels, palette, transparent);
	gl_draw_screen(true);
}

void sys_gl_swap(void)
{
	gl_swap();
}

void sys_gl_read_picture(uint8_t *rgb, int *w, int *h)
{
	SDL_Rect r = gl_picture_rect();
	*w = r.w;
	*h = r.h;
	if (!rgb)
		return;
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(r.x, r.y, r.w, r.h, GL_RGB, GL_UNSIGNED_BYTE, rgb);
	/* Bottom-up to top-down. */
	size_t row = (size_t)r.w * 3;
	uint8_t *tmp = malloc(row);
	for (int y = 0; y < r.h / 2; y++) {
		memcpy(tmp, rgb + y * row, row);
		memcpy(rgb + y * row, rgb + (size_t)(r.h - 1 - y) * row, row);
		memcpy(rgb + (size_t)(r.h - 1 - y) * row, tmp, row);
	}
	free(tmp);
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
			quit_requested = true;
			break;
		case SDL_MOUSEMOTION:
			mouse_x = e.motion.x;
			mouse_y = e.motion.y;
			map_mouse(&mouse_x, &mouse_y);
			break;
		case SDL_MOUSEBUTTONDOWN:
			if (e.button.button == SDL_BUTTON_LEFT) {
				mouse_x = e.button.x;
				mouse_y = e.button.y;
				map_mouse(&mouse_x, &mouse_y);
				mouse_down = true;
				in->mouse_pressed = true;
			}
			break;
		case SDL_MOUSEBUTTONUP:
			if (e.button.button == SDL_BUTTON_LEFT) {
				mouse_x = e.button.x;
				mouse_y = e.button.y;
				map_mouse(&mouse_x, &mouse_y);
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
	in->quit = quit_requested;
}

/* Windows virtual-key codes to SDL scancodes, for the keys the game asks about. */
static SDL_Scancode vk_to_scancode(int vk)
{
	if (vk >= 'A' && vk <= 'Z')
		return (SDL_Scancode)(SDL_SCANCODE_A + vk - 'A');
	if (vk >= '1' && vk <= '9')
		return (SDL_Scancode)(SDL_SCANCODE_1 + vk - '1');
	if (vk >= 0x70 && vk <= 0x7b)
		return (SDL_Scancode)(SDL_SCANCODE_F1 + vk - 0x70);
	if (vk >= 0x61 && vk <= 0x69)
		return (SDL_Scancode)(SDL_SCANCODE_KP_1 + vk - 0x61);
	switch (vk) {
	case '0': return SDL_SCANCODE_0;
	case 0x60: return SDL_SCANCODE_KP_0;
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

bool sys_quit_requested(void)
{
	return quit_requested;
}

static SDL_Joystick *joysticks[2];

bool sys_joystick_position(int n, unsigned *x, unsigned *y, unsigned *buttons)
{
	if (headless || n < 0 || n > 1)
		return false;
	if (!SDL_WasInit(SDL_INIT_JOYSTICK) && SDL_InitSubSystem(SDL_INIT_JOYSTICK) != 0)
		return false;
	SDL_JoystickUpdate();
	if (joysticks[n] && !SDL_JoystickGetAttached(joysticks[n])) {
		SDL_JoystickClose(joysticks[n]);
		joysticks[n] = NULL;
	}
	if (!joysticks[n]) {
		if (SDL_NumJoysticks() <= n)
			return false;
		joysticks[n] = SDL_JoystickOpen(n);
		if (!joysticks[n])
			return false;
	}
	SDL_Joystick *j = joysticks[n];
	*x = (unsigned)(SDL_JoystickGetAxis(j, 0) + 32768);
	*y = (unsigned)(SDL_JoystickGetAxis(j, 1) + 32768);
	*buttons = 0;
	int count = SDL_JoystickNumButtons(j);
	for (int i = 0; i < count && i < 32; i++) {
		if (SDL_JoystickGetButton(j, i))
			*buttons |= 1u << i;
	}
	return true;
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
	for (int i = 0; i < 4; i++) {
		if (sc == held_key[i] && SDL_GetTicks() < held_until[i])
			return true;
	}
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
