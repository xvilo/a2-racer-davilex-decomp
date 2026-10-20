#include "race/object3d.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "race/race_files.h"
#include "race/render.h"

vertex3d_t *vertices3d;
triangle3d_t *triangles3d;
frame3d_t *frames3d;
anim3d_t *anims3d;
object3d_t *objects3d;
int object_count;

static int vertex_capacity, next_vertex;
static int triangle_capacity, next_triangle;
static int frame3d_capacity, next_frame3d;
static int anim3d_capacity, next_anim3d;
static int next_object3d, object_update_counter;

static void fatal(const char *what)
{
	fprintf(stderr, "3d: %s\n", what);
	exit(1);
}

/* --- vertex.cpp ------------------------------------------------------------------ */

int alloc_vertices(int count)
{
	if (vertices3d)
		return 0;
	vertices3d = calloc((size_t)count, sizeof *vertices3d);
	if (!vertices3d)
		return 2000;
	vertex_capacity = count;
	return 0;
}

void free_vertices(void)
{
	free(vertices3d);
	vertices3d = NULL;
	vertex_capacity = next_vertex = 0;
}

/* New vertices come from the end of the used part, then from freed ones. */
int get_new_vertices(void)
{
	if (next_vertex < vertex_capacity)
		return next_vertex++;
	for (int i = 0; i < vertex_capacity; i++) {
		if (!vertices3d[i].in_use)
			return i;
	}
	fatal("no more free vertices");
	return 0;
}

/* --- triangle.cpp ---------------------------------------------------------------- */

int alloc_triangles(int count)
{
	if (triangles3d)
		return 0;
	triangles3d = calloc((size_t)count, sizeof *triangles3d);
	if (!triangles3d)
		return 2000;
	triangle_capacity = count;
	return 0;
}

void free_triangles(void)
{
	free(triangles3d);
	triangles3d = NULL;
	triangle_capacity = next_triangle = 0;
}

int get_new_triangles(void)
{
	if (next_triangle < triangle_capacity)
		return next_triangle++;
	for (int i = 0; i < triangle_capacity; i++) {
		if (!triangles3d[i].in_use)
			return i;
	}
	fatal("no more free triangles");
	return 0;
}

/* --- frame3d.cpp ----------------------------------------------------------------- */

int alloc_frames3d(int count)
{
	if (frames3d)
		return 0;
	frames3d = calloc((size_t)count, sizeof *frames3d);
	if (!frames3d)
		return 2000;
	frame3d_capacity = count;
	return 0;
}

void free_frames3d(void)
{
	for (int i = 0; i < frame3d_capacity; i++)
		free_frame3d(i);
	free(frames3d);
	frames3d = NULL;
	frame3d_capacity = next_frame3d = 0;
}

static int get_new_frame3d(void)
{
	if (next_frame3d < frame3d_capacity)
		return next_frame3d++;
	for (int i = 0; i < frame3d_capacity; i++) {
		if (!frames3d[i].in_use)
			return i;
	}
	fatal("no more free frames");
	return 0;
}

int init_frame3d(int vertex_count, int triangle_count)
{
	int f = get_new_frame3d();
	frame3d_t *fr = &frames3d[f];
	fr->vertices = calloc((size_t)vertex_count + 1, sizeof *fr->vertices);
	fr->triangles = calloc((size_t)triangle_count + 1, sizeof *fr->triangles);
	if (!fr->vertices || !fr->triangles)
		fatal("not enough memory for a frame");
	for (int i = 0; i < vertex_count; i++) {
		fr->vertices[i] = get_new_vertices();
		vertices3d[fr->vertices[i]].in_use = 1;
	}
	for (int i = 0; i < triangle_count; i++) {
		fr->triangles[i] = get_new_triangles();
		triangles3d[fr->triangles[i]].in_use = 1;
	}
	fr->vertex_count = vertex_count;
	fr->triangle_count = triangle_count;
	fr->in_use = 1;
	return f;
}

void free_frame3d(int frame)
{
	frame3d_t *fr = &frames3d[frame];
	if (!fr->in_use)
		return;
	for (int i = 0; i < fr->vertex_count; i++)
		vertices3d[fr->vertices[i]].in_use = 0;
	for (int i = 0; i < fr->triangle_count; i++)
		triangles3d[fr->triangles[i]].in_use = 0;
	free(fr->vertices);
	free(fr->triangles);
	memset(fr, 0, sizeof *fr);
}

/* --- anim3d.cpp ------------------------------------------------------------------ */

int alloc_3d_arrays(int anim_count, int frame_count, int vertex_count, int triangle_count)
{
	if (anims3d)
		return 0;
	anims3d = calloc((size_t)anim_count, sizeof *anims3d);
	if (!anims3d)
		return 2000;
	anim3d_capacity = anim_count;
	if (alloc_frames3d(frame_count) || alloc_vertices(vertex_count) ||
		alloc_triangles(triangle_count))
		return 2000;
	return 0;
}

void free_3d_arrays(void)
{
	for (int i = 0; i < anim3d_capacity; i++)
		free(anims3d[i].frames);
	free(anims3d);
	anims3d = NULL;
	anim3d_capacity = next_anim3d = 0;
	free_frames3d();
	free_vertices();
	free_triangles();
}

static int get_new_anim3d(void)
{
	if (next_anim3d < anim3d_capacity)
		return next_anim3d++;
	for (int i = 0; i < anim3d_capacity; i++) {
		if (!anims3d[i].in_use)
			return i;
	}
	fatal("no more free anims");
	return 0;
}

/* An anim whose frames are allocated straight away, all the same size. */
int init_static_anim3d(int frame_count, int vertex_count, int triangle_count)
{
	int a = get_new_anim3d();
	anims3d[a].frames = calloc((size_t)frame_count, sizeof(int));
	if (!anims3d[a].frames)
		fatal("not enough memory for an anim");
	for (int i = 0; i < frame_count; i++)
		anims3d[a].frames[i] = init_frame3d(vertex_count, triangle_count);
	anims3d[a].frame_count = frame_count;
	anims3d[a].in_use = 1;
	return a;
}

/* An anim whose frames are filled in later (load_object_mesh). */
int init_anim3d(int frame_count)
{
	int a = get_new_anim3d();
	anims3d[a].frames = calloc((size_t)frame_count, sizeof(int));
	if (!anims3d[a].frames)
		fatal("not enough memory for an anim");
	anims3d[a].frame_count = frame_count;
	anims3d[a].in_use = 1;
	return a;
}

void free_anim3d(int anim)
{
	anim3d_t *an = &anims3d[anim];
	if (!an->in_use)
		return;
	for (int i = 0; i < an->frame_count; i++)
		free_frame3d(an->frames[i]);
	free(an->frames);
	memset(an, 0, sizeof *an);
}

/* --- object3d.cpp ---------------------------------------------------------------- */

int alloc_objects3d(int count)
{
	if (objects3d)
		return 0;
	objects3d = calloc((size_t)count, sizeof *objects3d);
	if (!objects3d)
		return 2000;
	object_count = count;
	for (int i = 0; i < count; i++) {
		objects3d[i].visible = 1;
		objects3d[i].update_interval = -1;
		objects3d[i].texture = -1;
		matrix_identity(&objects3d[i].matrix);
	}
	return 0;
}

void free_objects3d(void)
{
	for (int i = 0; i < object_count; i++)
		mesh_free(objects3d[i].mesh);
	free(objects3d);
	objects3d = NULL;
	object_count = next_object3d = 0;
}

int get_new_object3d(void)
{
	if (next_object3d < object_count)
		return next_object3d++;
	for (int i = 0; i < object_count; i++) {
		if (!objects3d[i].in_use)
			return i;
	}
	fatal("no more free objects");
	return 0;
}

void set_object3d_matrix(int object, const matrix_t *m)
{
	if (object < 0 || object >= object_count)
		fatal("illegal object in set_object3d_matrix");
	objects3d[object].matrix = *m;
}

void check_objects3d_visibility(int min_x, int min_z, int max_x, int max_z)
{
	for (int i = 0; i < object_count; i++) {
		object3d_t *o = &objects3d[i];
		o->visible = o->in_use && !(o->x < (float)min_x || (float)max_x < o->x ||
			o->z < (float)min_z || (float)max_z < o->z);
	}
}

void execute_object_buffers(void)
{
	for (int i = 0; i < object_count; i++) {
		object3d_t *o = &objects3d[i];
		/* One counter for all objects, as in the original. */
		if (o->in_use && o->update_interval > 0 &&
			object_update_counter++ % o->update_interval == 0 && o->update) {
			if (o->update(i) != 0)
				fatal("error executing an object function");
		}
	}
}

frame3d_t *object_frame(int object)
{
	const object3d_t *o = &objects3d[object];
	return &frames3d[anims3d[o->anim].frames[o->frame]];
}

/* --- objio.cpp: .d3d files ------------------------------------------------------- */

static void read_mesh(const uint8_t *p, size_t size, frame3d_t *fr, int nv, int nt,
	const char *filename)
{
	if (size < 8 + (size_t)nv * sizeof(vertex3d_t) + (size_t)nt * sizeof(triangle3d_t)) {
		fprintf(stderr, "%s: file too short\n", filename);
		exit(1);
	}
	p += 8;
	for (int i = 0; i < nv; i++, p += sizeof(vertex3d_t))
		memcpy(&vertices3d[fr->vertices[i]], p, sizeof(vertex3d_t));
	for (int i = 0; i < nt; i++, p += sizeof(triangle3d_t))
		memcpy(&triangles3d[fr->triangles[i]], p, sizeof(triangle3d_t));
}

static const uint8_t *open_model(const char *filename, size_t *size, int *nv, int *nt)
{
	const uint8_t *p = race_file(filename, size);
	if (!p || *size < 8) {
		fprintf(stderr, "Unable to load object %s\n", filename);
		exit(1);
	}
	memcpy(nv, p, 4);
	memcpy(nt, p + 4, 4);
	return p;
}

int load_object_file(const char *filename)
{
	size_t size;
	int nv, nt;
	const uint8_t *p = open_model(filename, &size, &nv, &nt);
	int o = get_new_object3d();
	objects3d[o].in_use = 1;
	objects3d[o].anim = init_static_anim3d(1, nv, nt);
	objects3d[o].frame = 0;
	read_mesh(p, size, object_frame(o), nv, nt, filename);
	return o;
}

void load_object_mesh(int object, const char *filename)
{
	size_t size;
	int nv, nt;
	const uint8_t *p = open_model(filename, &size, &nv, &nt);
	object3d_t *o = &objects3d[object];
	o->in_use = 1;
	anims3d[o->anim].frames[o->frame] = init_frame3d(nv, nt);
	read_mesh(p, size, object_frame(object), nv, nt, filename);
}
