#include "race/weg.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "race/race_files.h"
#include "race/texture.h"

weg_section_t *weg_sections;
weg_chunk_t *weg_chunks;
int *weg_chunk_quads;
int weg_chunk_count;
int road_texture = -1, grass_texture = -1;

static int weg_section_capacity, weg_lane_chunks;
int weg_section_count; /* chunks allocated */
static int visible_first, visible_last;

/* 8 x 8 tiles of 1/8 in the grass texture (init_tiles): each tile's four corners. */
static float tile_u[64][4], tile_v[64][4];

static void init_tiles(void)
{
	for (int j = 0, i = 0; j < 8; j++) {
		for (int k = 0; k < 8; k++, i++) {
			float u = k * 0.125f, v = j * 0.125f;
			tile_u[i][0] = u;
			tile_u[i][1] = u;
			tile_u[i][2] = u + 0.125f;
			tile_u[i][3] = u + 0.125f;
			tile_v[i][0] = v + 0.125f;
			tile_v[i][1] = v;
			tile_v[i][2] = v;
			tile_v[i][3] = v + 0.125f;
		}
	}
}

int interpolate_weg_point(float x0, float z0, float x1, float z1)
{
	double a = x0 == x1 ? (z0 <= z1 ? 1.5707963267948966 : -1.5707963267948966)
		: atan2(z1 - z0, x1 - x0);
	return (int)((float)a * 1024.0 / 6.283185307);
}

float road_height_at(int chunk, float x, float z)
{
	if (chunk == -1)
		return 0.0f;
	const weg_section_t *s = &weg_sections[chunk];
	return -((s->plane_c * z + s->plane_a * x + s->plane_d) / s->plane_b);
}

static int new_vertex(void)
{
	int v = get_new_vertices();
	vertices3d[v].in_use = 1;
	return v;
}

static int *new_vertex_list(int n)
{
	int *list = calloc((size_t)n, sizeof *list);
	if (!list) {
		fprintf(stderr, "not enough memory for the road\n");
		exit(1);
	}
	for (int i = 0; i < n; i++)
		list[i] = new_vertex();
	return list;
}

/* A section with `ground_count` ground vertices per side (alloc_weg_lanes). */
static int alloc_weg_lanes(int ground_count)
{
	if (weg_lane_chunks + 1 >= weg_section_capacity) {
		fprintf(stderr, "not enough weg chunks (%d)\n", weg_section_capacity);
		exit(1);
	}
	weg_section_t *s = &weg_sections[weg_lane_chunks];
	s->ground_left = new_vertex_list(ground_count);
	s->buffer_3 = calloc((size_t)ground_count, sizeof(int));
	s->ground_right = new_vertex_list(ground_count);
	s->buffer_4 = calloc((size_t)ground_count, sizeof(int));
	s->ground_count = ground_count;
	s->point = new_vertex();
	s->edge_right = new_vertex();
	s->edge_left = new_vertex();
	for (int k = 0; k < 2; k++) {
		s->rail_right[k] = new_vertex();
		s->rail_left[k] = new_vertex();
		s->median_right[k] = new_vertex();
		s->median_left[k] = new_vertex();
	}
	return weg_lane_chunks++;
}

static void set_xyz(vertex3d_t *v, float x, float y, float z)
{
	v->x = x;
	v->y = y;
	v->z = z;
}

static void set_normal(vertex3d_t *v, const float n[3])
{
	v->nx = n[0];
	v->ny = n[1];
	v->nz = n[2];
}

/* Builds the chunk from centre point a to b (build_weg_sections); returns its index. */
static int build_weg_sections(int a, int b, int ground_count, int lanes)
{
	if (weg_chunk_count + 1 >= weg_section_count) {
		fprintf(stderr, "not enough weg elements (%d)\n", weg_section_count);
		exit(1);
	}
	int c = weg_chunk_count++;
	weg_chunk_t *ch = &weg_chunks[c];
	const vertex3d_t *pa = &vertices3d[a], *pb = &vertices3d[b];
	ch->start = a;
	ch->end = b;
	ch->lanes = lanes;
	ch->in_use = 1;
	ch->visible = 1;
	ch->section = alloc_weg_lanes(ground_count);
	weg_section_t *s = &weg_sections[ch->section];

	float dir[3] = { pb->x - pa->x, pb->y - pa->y, pb->z - pa->z };
	s->heading = interpolate_weg_point(pa->x, pa->z, pb->x, pb->z);
	normalize_vector(dir);
	/* The road's normal: straight up, made perpendicular to the direction. */
	float d = dir[2] * 0.0f + dir[0] * 0.0f + dir[1];
	float normal[3] = { 0.0f - dir[0] * d, 1.0f - dir[1] * d, 0.0f - d * dir[2] };
	float side[3];
	normalize_vector(normal);
	cross_product(side, normal, dir);
	normalize_vector(side);
	s->plane_a = normal[0];
	s->plane_b = normal[1];
	s->plane_c = normal[2];
	s->plane_d = -(s->plane_c * pa->z + s->plane_a * pa->x + s->plane_b * pa->y);
	if (s->plane_b == 0.0f)
		s->plane_b = 0.1f;
	memcpy(ch->side, side, sizeof side);
	memcpy(ch->dir, dir, sizeof dir);

	vertex3d_t *v = &vertices3d[s->point];
	set_xyz(v, pa->x, pa->y, pa->z);
	set_normal(v, normal);
	v->tu = 0.0f;
	v->tv = 0.0f;
	float w = ((float)lanes + 2.0f) * 30.0f;
	v = &vertices3d[s->edge_right];
	set_xyz(v, w * side[0] + pa->x, pa->y, w * side[2] + pa->z);
	set_normal(v, normal);
	v->tu = 1.0f;
	v->tv = 0.0f;
	v = &vertices3d[s->edge_left];
	set_xyz(v, pa->x - w * side[0], pa->y, pa->z - w * side[2]);
	set_normal(v, normal);
	v->tu = 1.0f;
	v->tv = 0.0f;

	/* Guard rails at the edges and the median by the centre, 5 high along the normal.
	 * The original's order of operations is kept: the top of the right median gets its
	 * x and z from the centre and the side only. */
	const vertex3d_t *left = &vertices3d[s->edge_left], *right = &vertices3d[s->edge_right];
	vertex3d_t *r;
	r = &vertices3d[s->rail_left[0]];
	memcpy(r, left, 32);
	set_normal(r, side);
	r = &vertices3d[s->rail_left[1]];
	memcpy(r, left, 32);
	set_xyz(r, normal[0] * 5.0f + r->x, normal[1] * 5.0f + r->y, normal[2] * 5.0f + r->z);
	set_normal(r, side);
	r = &vertices3d[s->rail_right[0]];
	memcpy(r, right, 32);
	set_normal(r, side);
	r = &vertices3d[s->rail_right[1]];
	memcpy(r, right, 32);
	set_xyz(r, normal[0] * 5.0f + r->x, normal[1] * 5.0f + r->y, normal[2] * 5.0f + r->z);
	set_normal(r, side);
	const vertex3d_t *centre = &vertices3d[s->point];
	r = &vertices3d[s->median_left[0]];
	memcpy(r, centre, 32);
	r->x = pa->x - side[0] * 5.0f;
	r->z = pa->z - side[2] * 5.0f;
	set_normal(r, side);
	r = &vertices3d[s->median_left[1]];
	memcpy(r, centre, 32);
	r->x = pa->x - side[0] * 5.0f;
	r->z = pa->z - side[2] * 5.0f;
	set_xyz(r, normal[0] * 5.0f + r->x, normal[1] * 5.0f + r->y, normal[2] * 5.0f + r->z);
	set_normal(r, side);
	r = &vertices3d[s->median_right[0]];
	memcpy(r, centre, 32);
	r->x = side[0] * 5.0f + pa->x;
	r->z = side[2] * 5.0f + pa->z;
	set_normal(r, side);
	r = &vertices3d[s->median_right[1]];
	memcpy(r, centre, 32);
	set_xyz(r, normal[0] * 5.0f + r->x, normal[1] * 5.0f + r->y, normal[2] * 5.0f + r->z);
	r->x = side[0] * 5.0f + pa->x;
	r->z = side[2] * 5.0f + pa->z;
	set_normal(r, side);

	/* The ground: flat at 0 beyond its first row, which follows the road's edge. */
	static const float up[3] = { 0.0f, 1.0f, 0.0f };
	float x = left->x, z = left->z;
	for (int j = 0; j < ground_count; j++) {
		r = &vertices3d[s->ground_left[j]];
		r->x = x;
		r->z = z;
		x -= side[0] * 60.0f;
		z -= side[2] * 60.0f;
		r->y = j == 0 ? pa->y : 0.0f;
		set_normal(r, up);
	}
	x = right->x;
	z = right->z;
	for (int j = 0; j < ground_count; j++) {
		r = &vertices3d[s->ground_right[j]];
		r->x = x;
		r->z = z;
		x += side[0] * 60.0f;
		z += side[2] * 60.0f;
		r->y = j == 0 ? pa->y : 0.0f;
		set_normal(r, up);
	}
	return c;
}

static void put(mesh_t *m, int *slot, int vertex, float u, float v)
{
	vertices3d[vertex].tu = u;
	vertices3d[vertex].tv = v;
	mesh_set_vertex(m, (*slot)++, &vertices3d[vertex]);
}

/* A side's guard rail or median: the grass texture's top row, stretched along the
 * chunk; both faces are drawn. */
static void put_wall(mesh_t *m, int *slot, const int a[2], const int b[2])
{
	put(m, slot, a[0], tile_u[0][0], tile_v[0][0]);
	put(m, slot, a[1], tile_u[0][1], tile_v[0][1]);
	put(m, slot, b[0], tile_u[7][3], tile_v[7][3]);
	put(m, slot, b[1], tile_u[7][2], tile_v[7][2]);
}

/* The chunk's mesh (build_weg_execute_buffers): `side_length` ground vertices per side
 * are used, tile textures from the track file. */
static mesh_t *build_weg_execute_buffers(int chunk, int side_length, const int *tex_left,
	const int *tex_right)
{
	if (side_length < 2) {
		fprintf(stderr, "weg: illegal side length %d\n", side_length);
		exit(1);
	}
	const weg_section_t *s0 = &weg_sections[weg_chunks[weg_chunk_quads[chunk]].section];
	const weg_section_t *s1 = &weg_sections[weg_chunks[weg_chunk_quads[chunk + 1]].section];
	int tiles = side_length - 1;
	mesh_t *m = mesh_new(side_length * 8 + 0x10, 4 + 16 + tiles * 4);
	int slot = 0;
	/* The road, centre to edge on each side, the texture once across each half. */
	put(m, &slot, s0->point, 0, 0);
	put(m, &slot, s0->edge_right, 1, 0);
	put(m, &slot, s1->point, 0, 1);
	put(m, &slot, s1->edge_right, 1, 1);
	put(m, &slot, s0->point, 0, 0);
	put(m, &slot, s0->edge_left, 1, 0);
	put(m, &slot, s1->point, 0, 1);
	put(m, &slot, s1->edge_left, 1, 1);
	put_wall(m, &slot, s0->rail_left, s1->rail_left);
	put_wall(m, &slot, s0->median_left, s1->median_left);
	put_wall(m, &slot, s0->rail_right, s1->rail_right);
	put_wall(m, &slot, s0->median_right, s1->median_right);
	for (int t = 0; t < tiles; t++) {
		int tile = tex_left[t] & 63;
		put(m, &slot, s0->ground_left[t + 1], tile_u[tile][0], tile_v[tile][0]);
		put(m, &slot, s0->ground_left[t], tile_u[tile][1], tile_v[tile][1]);
		put(m, &slot, s1->ground_left[t + 1], tile_u[tile][3], tile_v[tile][3]);
		put(m, &slot, s1->ground_left[t], tile_u[tile][2], tile_v[tile][2]);
	}
	for (int t = 0; t < tiles; t++) {
		int tile = tex_right[t] & 63;
		put(m, &slot, s0->ground_right[t], tile_u[tile][0], tile_v[tile][0]);
		put(m, &slot, s0->ground_right[t + 1], tile_u[tile][1], tile_v[tile][1]);
		put(m, &slot, s1->ground_right[t], tile_u[tile][3], tile_v[tile][3]);
		put(m, &slot, s1->ground_right[t + 1], tile_u[tile][2], tile_v[tile][2]);
	}

	mesh_texture(m, road_texture);
	mesh_triangle(m, 0, 2, 3);
	mesh_triangle(m, 0, 3, 1);
	mesh_triangle(m, 4, 7, 6);
	mesh_triangle(m, 4, 5, 7);
	mesh_texture(m, grass_texture);
	for (int b = 8; b < 24; b += 4) {
		mesh_triangle(m, b, b + 2, b + 3);
		mesh_triangle(m, b, b + 3, b + 1);
		mesh_triangle(m, b + 3, b + 2, b);
		mesh_triangle(m, b + 1, b + 3, b);
	}
	for (int j = 24; j < slot; j += 4) {
		mesh_triangle(m, j, j + 2, j + 3);
		mesh_triangle(m, j, j + 3, j + 1);
	}
	return m;
}

/* One point of the track file: x, z, a word, the height, then two lists of `n` tile
 * numbers (left and right ground). */
typedef struct {
	const uint8_t *p, *end;
} reader;

static int read_point(reader *r, vertex3d_t *v, int *n, int tex_left[64], int tex_right[64])
{
	int32_t y, count;
	if (r->end - r->p < 20)
		return -1;
	memcpy(&v->x, r->p, 4);
	memcpy(&v->z, r->p + 4, 4);
	memcpy(&y, r->p + 12, 4);
	memcpy(&count, r->p + 16, 4);
	r->p += 20;
	if (count < 0 || count > 64 || r->end - r->p < count * 8)
		return -1;
	for (int i = 0; i < count; i++) {
		memcpy(&tex_left[i], r->p + i * 4, 4);
		memcpy(&tex_right[i], r->p + (count + i) * 4, 4);
	}
	r->p += count * 8;
	v->y = (float)y;
	*n = count;
	return 0;
}

static void draw_weg(void)
{
	for (int i = visible_first; i < visible_last && i < weg_chunk_count; i++) {
		if (weg_chunks[i].visible)
			mesh_draw(weg_chunks[i].mesh, NULL);
	}
}

void set_visible_weg_chunks(int first, int last)
{
	if (first < 0)
		last = 0;
	visible_first = first < 0 ? 0 : first;
	visible_last = last;
}

int load_weg_data(const char *filename, int max_side)
{
	size_t size;
	uint8_t *data = race_disc_file(filename, &size);
	if (!data || size < 4) {
		fprintf(stderr, "could not open weg data %s\n", filename);
		return -1;
	}
	int32_t count;
	memcpy(&count, data, 4);
	init_tiles();
	weg_sections = calloc((size_t)count, sizeof *weg_sections);
	weg_section_capacity = count;
	weg_lane_chunks = 0;
	if (count <= max_side)
		max_side = ((count - 1) / 2) * 2;
	weg_chunks = calloc((size_t)count, sizeof *weg_chunks);
	weg_chunk_quads = calloc((size_t)count, sizeof *weg_chunk_quads);
	weg_section_count = count;
	weg_chunk_count = 0;

	int a = new_vertex(), b = new_vertex(), n;
	int tex_left[64], tex_right[64];
	reader r = { data + 4, data + size };
	vertices3d[a].y = vertices3d[b].y = 0.0f;
	if (read_point(&r, &vertices3d[a], &n, tex_left, tex_right))
		goto bad;
	for (int i = 0; i < count - 1; i++) {
		if (read_point(&r, &vertices3d[b], &n, tex_left, tex_right))
			goto bad;
		weg_chunk_quads[i] = build_weg_sections(a, b, n, 2);
		vertices3d[a].x = vertices3d[b].x;
		vertices3d[a].y = vertices3d[b].y;
		vertices3d[a].z = vertices3d[b].z;
	}

	/* Second pass: the meshes, with each chunk's tiles from the point after it. The
	 * original reads WEG/COMPLETE from d3d_lib.ini; without it half the ground strip is
	 * drawn. The last chunk would join the first section (its next entry in
	 * weg_chunk_quads is 0), so it gets no mesh. */
	r.p = data + 4;
	read_point(&r, &vertices3d[a], &n, tex_left, tex_right);
	for (int i = 0; i < count - 1; i++) {
		read_point(&r, &vertices3d[a], &n, tex_left, tex_right);
		if (i + 1 < count - 1)
			weg_chunks[weg_chunk_quads[i]].mesh =
				build_weg_execute_buffers(i, n / 2, tex_left, tex_right);
	}
	free(data);
	set_weg_renderer(draw_weg);
	visible_first = 0;
	visible_last = weg_chunk_count;
	return 0;
bad:
	fprintf(stderr, "error reading weg data %s\n", filename);
	free(data);
	return -1;
}

void free_weg_data(void)
{
	for (int i = 0; i < weg_section_capacity; i++) {
		free(weg_sections[i].ground_left);
		free(weg_sections[i].ground_right);
		free(weg_sections[i].buffer_3);
		free(weg_sections[i].buffer_4);
	}
	for (int i = 0; i < weg_chunk_count; i++)
		mesh_free(weg_chunks[i].mesh);
	free(weg_sections);
	free(weg_chunks);
	free(weg_chunk_quads);
	weg_sections = NULL;
	weg_chunks = NULL;
	weg_chunk_quads = NULL;
	weg_chunk_count = weg_section_capacity = 0;
	set_weg_renderer(NULL);
}

/* --- wegtools.cpp ---------------------------------------------------------------- */

/* Which side of the line across the road at a section (right edge to left edge) the
 * point is on, in whole units as the original. */
static int point_side_of_chunk_edge(int section, float x, float z)
{
	const weg_section_t *s = &weg_sections[section];
	int x1 = (int)vertices3d[s->edge_right].x, z1 = (int)vertices3d[s->edge_right].z;
	int x2 = (int)vertices3d[s->edge_left].x, z2 = (int)vertices3d[s->edge_left].z;
	int d = ((int)x - x1) * (z2 - z1) - (x2 - x1) * ((int)z - z1);
	return d > 0 ? 1 : d < 0 ? -1 : 0;
}

static int point_between_chunk_edges(int chunk, float x, float z)
{
	return point_side_of_chunk_edge(chunk, x, z) != point_side_of_chunk_edge(chunk + 1, x, z);
}

int update_chunk_for_position(int chunk, float x, float z)
{
	if (chunk < 0 || weg_section_capacity <= chunk) {
		fprintf(stderr, "chunk %d out of array bounds (%d)\n", chunk, weg_section_capacity);
		exit(1);
	}
	/* Outwards from the chunk: chunk, chunk + 1, chunk - 1, chunk + 2, ... The last
	 * section's neighbour does not exist, so the search stays below it. */
	int down = chunk, up = chunk + 1;
	while (down > 0 || up < weg_chunk_count) {
		if (down > 0) {
			if (point_between_chunk_edges(weg_chunk_quads[down], x, z))
				return weg_chunk_quads[down];
			down--;
		}
		if (up < weg_chunk_count) {
			if (point_between_chunk_edges(weg_chunk_quads[up], x, z))
				return weg_chunk_quads[up];
			up++;
		}
	}
	fprintf(stderr, "weg chunk is -1\n");
	return chunk;
}
