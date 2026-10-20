#include "race/render.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform/gl.h"
#include "platform/sys.h"
#include "race/texture.h"

static int viewport_x, viewport_y, viewport_w = 640, viewport_h = 480;
static int background_texture = -1;
static matrix_t view_rotation, view_translation;
static render_hook weg_renderer;

mesh_t *mesh_new(int vertex_count, int triangle_capacity)
{
	mesh_t *m = calloc(1, sizeof *m);
	if (!m)
		return NULL;
	m->vertex_count = vertex_count;
	m->vertices = calloc((size_t)vertex_count + 1, sizeof *m->vertices);
	m->index_capacity = triangle_capacity * 3;
	m->indices = calloc((size_t)m->index_capacity + 1, sizeof *m->indices);
	return m;
}

void mesh_free(mesh_t *m)
{
	if (!m)
		return;
	free(m->vertices);
	free(m->indices);
	free(m);
}

void mesh_set_vertex(mesh_t *m, int i, const vertex3d_t *v)
{
	mesh_vertex *d = &m->vertices[i];
	d->x = v->x;
	d->y = v->y;
	d->z = v->z;
	d->u = v->tu;
	d->v = v->tv;
}

void mesh_texture(mesh_t *m, int texture)
{
	if (m->batch_count == 4) {
		fprintf(stderr, "mesh: too many textures\n");
		exit(1);
	}
	mesh_batch *b = &m->batches[m->batch_count++];
	b->texture = texture;
	b->first = m->index_count;
	b->count = 0;
}

void mesh_triangle(mesh_t *m, int a, int b, int c)
{
	if (!m->batch_count)
		mesh_texture(m, -1);
	if (m->index_count + 3 > m->index_capacity || a >= m->vertex_count ||
		b >= m->vertex_count || c >= m->vertex_count) {
		fprintf(stderr, "mesh: triangle out of range\n");
		exit(1);
	}
	m->indices[m->index_count++] = (uint16_t)a;
	m->indices[m->index_count++] = (uint16_t)b;
	m->indices[m->index_count++] = (uint16_t)c;
	m->batches[m->batch_count - 1].count += 3;
}

void mesh_draw(const mesh_t *m, const matrix_t *world)
{
	if (!m || !m->index_count)
		return;
	/* modelview = world * view: v * World * View, as Direct3D applies them. */
	matrix_t mv = view_rotation;
	matrix_multiply(&mv, &view_translation);
	if (world)
		matrix_multiply(&mv, world);
	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf(mv.m);
	glVertexPointer(3, GL_FLOAT, sizeof(mesh_vertex), &m->vertices[0].x);
	glTexCoordPointer(2, GL_FLOAT, sizeof(mesh_vertex), &m->vertices[0].u);
	for (int i = 0; i < m->batch_count; i++) {
		const mesh_batch *b = &m->batches[i];
		if (!b->count)
			continue;
		if (b->texture >= 0) {
			glEnable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, texture_table[b->texture].gl);
		}
		else {
			glDisable(GL_TEXTURE_2D);
		}
		glDrawElements(GL_TRIANGLES, b->count, GL_UNSIGNED_SHORT, &m->indices[b->first]);
	}
}

void change_viewport_size(int x, int y, int w, int h)
{
	viewport_x = x;
	viewport_y = y;
	viewport_w = w;
	viewport_h = h;
}

void set_background(const char *filename)
{
	background_texture = load_texture(filename, 0);
}

void set_view(const matrix_t *rotation, const matrix_t *translation)
{
	view_rotation = *rotation;
	view_translation = *translation;
}

void set_weg_renderer(render_hook draw)
{
	weg_renderer = draw;
}

void build_object_execute_buffer(int object)
{
	object3d_t *o = &objects3d[object];
	const frame3d_t *fr = object_frame(object);
	mesh_free(o->mesh);
	o->mesh = mesh_new(fr->vertex_count, fr->triangle_count);
	for (int i = 0; i < fr->vertex_count; i++)
		mesh_set_vertex(o->mesh, i, &vertices3d[fr->vertices[i]]);
	mesh_texture(o->mesh, o->texture);
	for (int i = 0; i < fr->triangle_count; i++) {
		const triangle3d_t *t = &triangles3d[fr->triangles[i]];
		mesh_triangle(o->mesh, t->v1, t->v2, t->v3);
	}
}

/* The viewport's background material: the picture stretched over the 3D area. */
static void draw_background(void)
{
	if (background_texture < 0)
		return;
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, 1, 1, 0, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, texture_table[background_texture].gl);
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0);
	glVertex2f(0, 0);
	glTexCoord2f(1, 0);
	glVertex2f(1, 0);
	glTexCoord2f(1, 1);
	glVertex2f(1, 1);
	glTexCoord2f(0, 1);
	glVertex2f(0, 1);
	glEnd();
}

static void set_projection(void)
{
	/* Direct3D's projection [2 0 0 0; 0 2 0 0; 0 0 1 1; 0 0 -1 0] with the viewport's
	 * scale (its height in pixels), as OpenGL clip coordinates (z from -1). */
	float aspect = (float)viewport_w / (float)viewport_h;
	matrix_t p;
	memset(&p, 0, sizeof p);
	p.m[0] = 4.0f / aspect;
	p.m[5] = 4.0f;
	p.m[10] = 1.0f;
	p.m[11] = 1.0f;
	p.m[14] = -2.0f;
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(p.m);
}

void render_3d_frame(void)
{
	execute_object_buffers();
	sys_gl_viewport(viewport_x, viewport_y, viewport_w, viewport_h);
	glClear(GL_DEPTH_BUFFER_BIT);
	draw_background();

	set_projection();
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glDepthMask(GL_TRUE);
	glEnable(GL_CULL_FACE);
	glFrontFace(GL_CW);
	glCullFace(GL_BACK);
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.5f);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	for (int i = 0; i < object_count; i++) {
		const object3d_t *o = &objects3d[i];
		if (o->in_use && o->visible)
			mesh_draw(o->mesh, &o->matrix);
	}
	if (weg_renderer)
		weg_renderer();
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_TEXTURE_2D);
}
