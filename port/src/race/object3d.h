/* The 3D object pools (vertex.cpp, triangle.cpp, frame3d.cpp, anim3d.cpp, object3d.cpp).
 * Everything 3D comes from these: a frame is a mesh (lists of vertex and triangle
 * indices into the pools), an anim a list of frames, an object places an anim's frame
 * in the world. The layouts follow the original; vertex3d_t and triangle3d_t are
 * stored as is in the .d3d model files. */
#ifndef A2_OBJECT3D_H
#define A2_OBJECT3D_H

#include <stdint.h>

#include "race/math3d.h"

#pragma pack(push, 1)
/* A D3DVERTEX and an in-use flag. */
typedef struct {
	float x, y, z;
	float nx, ny, nz;
	float tu, tv;
	uint8_t in_use;
} vertex3d_t;

/* A D3DTRIANGLE (indices into the frame's vertex list) and an in-use flag. */
typedef struct {
	uint16_t v1, v2, v3;
	uint16_t flags; /* D3DTRIFLAG_* */
	uint8_t in_use;
} triangle3d_t;
#pragma pack(pop)

typedef char vertex3d_size_check[sizeof(vertex3d_t) == 0x21 ? 1 : -1];
typedef char triangle3d_size_check[sizeof(triangle3d_t) == 9 ? 1 : -1];

typedef struct {
	int vertex_count;
	int *vertices;  /* indices into vertices3d */
	int triangle_count;
	int *triangles; /* indices into triangles3d */
	uint8_t in_use;
} frame3d_t;

typedef struct {
	int frame_count;
	int *frames;    /* indices into frames3d */
	uint8_t in_use;
} anim3d_t;

typedef struct mesh_t mesh_t;

/* object3d_t: a placed 3D object. */
typedef struct {
	int anim, frame;
	float x, y, z;    /* position */
	float vx, vy, vz; /* velocity (knocked-over pionnen) */
	int (*update)(int object); /* every update_interval frames */
	matrix_t matrix;  /* world matrix (a D3DMATRIXHANDLE in the original) */
	int field_28;
	int texture;      /* index into the texture table, -1 for none */
	mesh_t *mesh;     /* the frame ready to draw (the execute buffer in the original) */
	uint8_t in_use;
	uint8_t visible;
	uint8_t field_36;
	int opponent;     /* index into opponent_data */
	int field_3b, field_3f, field_43;
	int update_interval;
} object3d_t;

extern vertex3d_t *vertices3d;
extern triangle3d_t *triangles3d;
extern frame3d_t *frames3d;
extern anim3d_t *anims3d;
extern object3d_t *objects3d;
extern int object_count;

int alloc_vertices(int count);
void free_vertices(void);
int get_new_vertices(void);
int alloc_triangles(int count);
void free_triangles(void);
int get_new_triangles(void);
int alloc_frames3d(int count);
void free_frames3d(void);
int init_frame3d(int vertex_count, int triangle_count);
void free_frame3d(int frame);
/* The anim, frame, vertex and triangle pools at once (alloc_3d_arrays). */
int alloc_3d_arrays(int anim_count, int frame_count, int vertex_count, int triangle_count);
void free_3d_arrays(void);
int init_static_anim3d(int frame_count, int vertex_count, int triangle_count);
int init_anim3d(int frame_count);
void free_anim3d(int anim);

int alloc_objects3d(int count);
void free_objects3d(void);
int get_new_object3d(void);
void set_object3d_matrix(int object, const matrix_t *m);
/* Objects outside the rectangle (x, z) are hidden. */
void check_objects3d_visibility(int min_x, int min_z, int max_x, int max_z);
/* Runs the objects' update functions (each every update_interval calls). */
void execute_object_buffers(void);
/* The current frame of an object. */
frame3d_t *object_frame(int object);

/* objio.cpp: a .d3d model file (vertex count, triangle count, the vertices, the
 * triangles) as a new object, or into an existing object's current frame. */
int load_object_file(const char *filename);
void load_object_mesh(int object, const char *filename);

#endif
