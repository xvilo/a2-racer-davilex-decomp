/* Drawing the 3D scene (d3dlib.cpp, viewport.cpp, exec.cpp). The original built a
 * Direct3D execute buffer per object and per road chunk; the port keeps a mesh_t with
 * the same vertices and triangles and draws it with OpenGL.
 *
 * The original's settings, kept: no lighting (vertices are drawn white, the texture
 * as is), nearest-pixel textures, z-buffer, back faces (counter-clockwise on screen)
 * culled, and its projection: vertically tan(fov/2) = 0.25, horizontally that times the
 * viewport's aspect, near plane at 1 and no far plane. */
#ifndef A2_RENDER_H
#define A2_RENDER_H

#include <stdint.h>

#include "race/math3d.h"
#include "race/object3d.h"

typedef struct {
	float x, y, z, u, v;
} mesh_vertex;

/* Triangles drawn with one texture. */
typedef struct {
	int texture;
	int first, count; /* range of indices */
} mesh_batch;

struct mesh_t {
	int vertex_count;
	mesh_vertex *vertices;
	int index_count, index_capacity;
	uint16_t *indices;
	int batch_count;
	mesh_batch batches[4];
};

mesh_t *mesh_new(int vertex_count, int triangle_capacity);
void mesh_free(mesh_t *m);
void mesh_set_vertex(mesh_t *m, int i, const vertex3d_t *v);
/* Following triangles use this texture (-1: untextured). */
void mesh_texture(mesh_t *m, int texture);
void mesh_triangle(mesh_t *m, int a, int b, int c);
void mesh_draw(const mesh_t *m, const matrix_t *world);

/* change_viewport_size: the 3D picture's rectangle in the 640x480 frame. */
void change_viewport_size(int x, int y, int w, int h);
void set_background(const char *filename);
/* set_camera's two matrices: the rotation and the (negated) camera position. */
void set_view(const matrix_t *rotation, const matrix_t *translation);

/* build_object_execute_buffer: the object's current frame as a mesh. */
void build_object_execute_buffer(int object);
/* Draws the background, the visible objects and the road (render_3d_frame). */
void render_3d_frame(void);

/* The road draws itself after the objects (execute_weg_buffer). */
typedef void (*render_hook)(void);
void set_weg_renderer(render_hook draw);

#endif
