#include "platform/gl.h"

#include <SDL.h>

#define A2_GL_DEFINE(ret, name, args) ret (APIENTRY *a2_gl##name) args;
A2_GL_FUNCS(A2_GL_DEFINE)
#undef A2_GL_DEFINE

bool gl_load(void)
{
	bool ok = true;
#define A2_GL_LOAD(ret, name, args) \
	*(void **)&a2_gl##name = SDL_GL_GetProcAddress("gl" #name); \
	if (!a2_gl##name) { \
		SDL_Log("OpenGL: gl" #name " missing"); \
		ok = false; \
	}
	A2_GL_FUNCS(A2_GL_LOAD)
#undef A2_GL_LOAD
	return ok;
}
