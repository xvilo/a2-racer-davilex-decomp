/* The OpenGL functions the port uses, loaded through SDL at run time so that nothing
 * links against an OpenGL library (the same code runs with Windows' opengl32, macOS'
 * OpenGL framework and Mesa). Fixed-function OpenGL 1.x/2.1, which all three offer. */
#ifndef A2_GL_H
#define A2_GL_H

#include <SDL_opengl.h>

#include <stdbool.h>

#define A2_GL_FUNCS(X) \
	X(void, Viewport, (GLint x, GLint y, GLsizei w, GLsizei h)) \
	X(void, Scissor, (GLint x, GLint y, GLsizei w, GLsizei h)) \
	X(void, ClearColor, (GLfloat r, GLfloat g, GLfloat b, GLfloat a)) \
	X(void, ClearDepth, (GLdouble d)) \
	X(void, Clear, (GLbitfield mask)) \
	X(void, Enable, (GLenum cap)) \
	X(void, Disable, (GLenum cap)) \
	X(void, Hint, (GLenum target, GLenum mode)) \
	X(void, GenTextures, (GLsizei n, GLuint *textures)) \
	X(void, DeleteTextures, (GLsizei n, const GLuint *textures)) \
	X(void, BindTexture, (GLenum target, GLuint texture)) \
	X(void, TexImage2D, (GLenum target, GLint level, GLint internalformat, GLsizei w, GLsizei h, \
		GLint border, GLenum format, GLenum type, const void *pixels)) \
	X(void, TexSubImage2D, (GLenum target, GLint level, GLint x, GLint y, GLsizei w, GLsizei h, \
		GLenum format, GLenum type, const void *pixels)) \
	X(void, TexParameteri, (GLenum target, GLenum pname, GLint param)) \
	X(void, TexEnvi, (GLenum target, GLenum pname, GLint param)) \
	X(void, PixelStorei, (GLenum pname, GLint param)) \
	X(void, MatrixMode, (GLenum mode)) \
	X(void, LoadIdentity, (void)) \
	X(void, LoadMatrixf, (const GLfloat *m)) \
	X(void, MultMatrixf, (const GLfloat *m)) \
	X(void, Ortho, (GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f)) \
	X(void, Begin, (GLenum mode)) \
	X(void, End, (void)) \
	X(void, TexCoord2f, (GLfloat s, GLfloat t)) \
	X(void, Vertex2f, (GLfloat x, GLfloat y)) \
	X(void, Color4f, (GLfloat r, GLfloat g, GLfloat b, GLfloat a)) \
	X(void, EnableClientState, (GLenum array)) \
	X(void, DisableClientState, (GLenum array)) \
	X(void, VertexPointer, (GLint size, GLenum type, GLsizei stride, const void *ptr)) \
	X(void, TexCoordPointer, (GLint size, GLenum type, GLsizei stride, const void *ptr)) \
	X(void, DrawElements, (GLenum mode, GLsizei count, GLenum type, const void *indices)) \
	X(void, DrawArrays, (GLenum mode, GLint first, GLsizei count)) \
	X(void, DepthFunc, (GLenum func)) \
	X(void, DepthMask, (GLboolean flag)) \
	X(void, BlendFunc, (GLenum s, GLenum d)) \
	X(void, AlphaFunc, (GLenum func, GLclampf ref)) \
	X(void, CullFace, (GLenum mode)) \
	X(void, FrontFace, (GLenum mode)) \
	X(void, Fogi, (GLenum pname, GLint param)) \
	X(void, Fogf, (GLenum pname, GLfloat param)) \
	X(void, Fogfv, (GLenum pname, const GLfloat *params)) \
	X(void, ReadPixels, (GLint x, GLint y, GLsizei w, GLsizei h, GLenum format, GLenum type, \
		void *pixels)) \
	X(void, ReadBuffer, (GLenum mode)) \
	X(GLenum, GetError, (void)) \
	X(const GLubyte *, GetString, (GLenum name))

/* SDL_opengl.h declares the functions themselves; the pointers get their own names and
 * the gl* names are redirected to them. */
#define A2_GL_DECLARE(ret, name, args) extern ret (APIENTRY *a2_gl##name) args;
A2_GL_FUNCS(A2_GL_DECLARE)
#undef A2_GL_DECLARE

#define glViewport a2_glViewport
#define glScissor a2_glScissor
#define glClearColor a2_glClearColor
#define glClearDepth a2_glClearDepth
#define glClear a2_glClear
#define glEnable a2_glEnable
#define glDisable a2_glDisable
#define glHint a2_glHint
#define glGenTextures a2_glGenTextures
#define glDeleteTextures a2_glDeleteTextures
#define glBindTexture a2_glBindTexture
#define glTexImage2D a2_glTexImage2D
#define glTexSubImage2D a2_glTexSubImage2D
#define glTexParameteri a2_glTexParameteri
#define glTexEnvi a2_glTexEnvi
#define glPixelStorei a2_glPixelStorei
#define glMatrixMode a2_glMatrixMode
#define glLoadIdentity a2_glLoadIdentity
#define glLoadMatrixf a2_glLoadMatrixf
#define glMultMatrixf a2_glMultMatrixf
#define glOrtho a2_glOrtho
#define glBegin a2_glBegin
#define glEnd a2_glEnd
#define glTexCoord2f a2_glTexCoord2f
#define glVertex2f a2_glVertex2f
#define glColor4f a2_glColor4f
#define glEnableClientState a2_glEnableClientState
#define glDisableClientState a2_glDisableClientState
#define glVertexPointer a2_glVertexPointer
#define glTexCoordPointer a2_glTexCoordPointer
#define glDrawElements a2_glDrawElements
#define glDrawArrays a2_glDrawArrays
#define glDepthFunc a2_glDepthFunc
#define glDepthMask a2_glDepthMask
#define glBlendFunc a2_glBlendFunc
#define glAlphaFunc a2_glAlphaFunc
#define glCullFace a2_glCullFace
#define glFrontFace a2_glFrontFace
#define glFogi a2_glFogi
#define glFogf a2_glFogf
#define glFogfv a2_glFogfv
#define glReadPixels a2_glReadPixels
#define glReadBuffer a2_glReadBuffer
#define glGetError a2_glGetError
#define glGetString a2_glGetString

/* Loads the functions for the current context; false if one is missing. */
bool gl_load(void);

#endif
