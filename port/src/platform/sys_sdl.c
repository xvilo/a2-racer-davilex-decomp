#include "platform/sys.h"

#include <SDL.h>

#include "gfx/screen.h"

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static uint32_t rgba[SCREEN_W * SCREEN_H];
static bool headless;

bool sys_init(const char *title, const sys_options *opt)
{
	headless = opt->hidden;
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
	in->key = KEY_NONE;
	if (headless)
		return;
	SDL_Event e;
	while (SDL_PollEvent(&e)) {
		switch (e.type) {
		case SDL_QUIT:
			in->quit = true;
			break;
		case SDL_MOUSEMOTION:
			in->mouse_x = e.motion.x;
			in->mouse_y = e.motion.y;
			break;
		case SDL_MOUSEBUTTONDOWN:
			if (e.button.button == SDL_BUTTON_LEFT)
				in->mouse_down = true;
			break;
		case SDL_MOUSEBUTTONUP:
			if (e.button.button == SDL_BUTTON_LEFT) {
				in->mouse_down = false;
				in->mouse_clicked = true;
			}
			break;
		case SDL_KEYDOWN:
			in->key = map_key(e.key.keysym.sym);
			break;
		}
	}
}

uint32_t sys_ticks(void)
{
	return SDL_GetTicks();
}

void sys_sleep(uint32_t ms)
{
	SDL_Delay(ms);
}
