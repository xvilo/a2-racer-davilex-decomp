/* The road ("weg", weg.cpp). A track file (ETAPPE1..3.TRK) is a list of centre points;
 * each pair of consecutive points is a chunk. A section sits at each point: the road's
 * centre, its right and left edges ((lanes + 2) * 30 out), a guard rail at each edge and
 * a median at 5 either side of the centre (both 5 high), and a strip of 60-wide ground
 * tiles on each side. The ground is flat at height 0; only its first row follows the
 * road, so a raised road slopes down to it. */
#ifndef A2_WEG_H
#define A2_WEG_H

#include "race/render.h"

typedef struct {
	int point;          /* the centre (vertex index) */
	int edge_right;     /* centre + width * side */
	int edge_left;
	int rail_left[2];   /* foot and top of the guard rail at the left edge */
	int median_left[2]; /* centre - 5 * side */
	int rail_right[2];
	int median_right[2];
	int *ground_left;   /* ground_count vertices from the left edge outwards */
	int *ground_right;
	int *buffer_3, *buffer_4;
	int ground_count;
	int heading;        /* 0..1023 */
	float plane_a, plane_b, plane_c, plane_d; /* height = -(a*x + c*z + d) / b */
} weg_section_t;

typedef struct {
	int lanes;          /* 2 */
	int field_04;
	int section;        /* index into weg_sections */
	int start, end;     /* vertex indices of the two centre points */
	int in_use;
	int visible;        /* in view this frame (weg_chunk_vertex_lists) */
	float side[3];      /* unit vector to the right, across the road */
	float dir[3];       /* unit vector along the road */
	mesh_t *mesh;       /* the execute buffer in the original */
} weg_chunk_t;

extern weg_section_t *weg_sections;
extern weg_chunk_t *weg_chunks;
extern int *weg_chunk_quads; /* chunk index per track point */
extern int weg_chunk_count;
extern int road_texture, grass_texture;

/* Loads a track; `max_side` limits the ground strips (20 in the original). */
int load_weg_data(const char *filename, int max_side);
void free_weg_data(void);
/* Which chunks to draw: [first, last). */
void set_visible_weg_chunks(int first, int last);
/* The road's height at (x, z) on a chunk's plane. */
float road_height_at(int chunk, float x, float z);
/* wegtools.cpp: the chunk containing (x, z), searched outwards from `chunk`. */
int update_chunk_for_position(int chunk, float x, float z);
/* heading of the line (x0, z0) to (x1, z1) in 0..1023 units (interpolate_weg_point). */
int interpolate_weg_point(float x0, float z0, float x1, float z1);

#endif
