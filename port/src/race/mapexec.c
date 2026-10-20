#include "race/mapexec.h"

#include "race/object3d.h"
#include "race/weg.h"

/* Tiles of 12 * 30 units, 6 of them across the box (tile_size, DAT_004596d8..e4). */
enum { TILE_SIZE = 12, TILE_SCALE = 12, TILES_X = 6, TILES_Z = 6, MAP_MAX = 200000 };

static int quad[8]; /* the wedge's points, as the original stores them */
static int view_min_x, view_max_x, view_min_z, view_max_z;

void set_view_quad(int x1, int z1, int x2, int z2, int x3, int z3)
{
	/* DAT_00477024..40: line (x2,z2)-(x1,z1), and line (x3,z3)-(x2,z2). */
	quad[0] = x2;
	quad[1] = z2;
	quad[2] = x1;
	quad[3] = z1;
	quad[4] = x3;
	quad[5] = z3;
	quad[6] = x2;
	quad[7] = z2;
}

/* point_side_of_line: which side of the line from (l[0], l[1]) to (l[2], l[3]). */
static int point_side_of_line(const int l[4], int x, int z)
{
	int dx = l[2] - l[0], dz = l[3] - l[1];
	int s = (x - l[0]) * dz - dx * (z - l[1]);
	return s > 0 ? 1 : s < 0 ? -1 : 0;
}

static int point_in_view_quad(int x, int z)
{
	return point_side_of_line(&quad[4], x, z) < 0 && point_side_of_line(&quad[0], x, z) < 0;
}

int view_octant(float x, float z)
{
	if (x < -0.5f)
		return z < -0.5f ? 5 : z > 0.5f ? 7 : 6;
	if (x > 0.5f)
		return z < -0.5f ? 3 : z > 0.5f ? 1 : 2;
	return z < -0.5f ? 4 : z > 0.5f ? 0 : 1;
}

void update_visible_tiles(int x, int z, int octant)
{
	int w = TILE_SIZE * TILES_X, h = TILE_SCALE * TILES_Z;
	int lx = 0, rx = 0, lz = 0, hz = 0;
	switch (octant) {
	case 0: lx = x - w / 2; rx = x + w / 2; hz = z + h - 2 * TILE_SCALE; lz = z - 2 * TILE_SCALE; break;
	case 1: lx = x - 2 * TILE_SIZE; rx = x + w - 2 * TILE_SIZE; hz = z + h - 2 * TILE_SCALE; lz = z - 2 * TILE_SCALE; break;
	case 2: lx = x - 2 * TILE_SIZE; rx = x + w - 2 * TILE_SIZE; hz = z + h / 2; lz = z - h / 2; break;
	case 3: lx = x - 2 * TILE_SIZE; rx = x + w - 2 * TILE_SIZE; hz = z + 2 * TILE_SCALE; lz = z - h + 2 * TILE_SCALE; break;
	case 4: lx = x - w / 2; rx = x + w / 2; hz = z + 2 * TILE_SCALE; lz = z - h + 2 * TILE_SCALE; break;
	case 5: lx = x - w + 2 * TILE_SIZE; rx = x + 2 * TILE_SIZE; hz = z + 2 * TILE_SCALE; lz = z - h + 2 * TILE_SCALE; break;
	case 6: lx = x - w + 2 * TILE_SIZE; rx = x + 2 * TILE_SIZE; hz = z + h / 2; lz = z - h / 2; break;
	case 7: lx = x - w + 2 * TILE_SIZE; rx = x + 2 * TILE_SIZE; hz = z + h - 2 * TILE_SCALE; lz = z - 2 * TILE_SCALE; break;
	}
	if (lx < 0) {
		lx = 0;
		rx = w;
	}
	if (lz < 0) {
		lz = 0;
		hz = h;
	}
	if (MAP_MAX < hz) {
		lz = MAP_MAX - h;
		hz = MAP_MAX;
	}
	if (MAP_MAX < rx) {
		lx = MAP_MAX - w;
		rx = MAP_MAX;
	}
	view_min_x = lx * 30;
	view_max_x = rx * 30;
	view_min_z = lz * 30;
	view_max_z = hz * 30;
	check_objects3d_visibility(view_min_x, view_min_z, view_max_x, view_max_z);
}

static int in_box(int vertex)
{
	const vertex3d_t *v = &vertices3d[vertex];
	return (float)view_min_x < v->x && v->x < (float)view_max_x && (float)view_min_z < v->z &&
		v->z < (float)view_max_z;
}

static int in_wedge(int vertex)
{
	return point_in_view_quad((int)vertices3d[vertex].x, (int)vertices3d[vertex].z);
}

void get_road_height(void)
{
	int first = -1, last = -1;
	/* The original tests points up to the third-last: every chunk but the last. */
	for (int i = 0; i < weg_chunk_count - 1; i++) {
		weg_chunk_t *ch = &weg_chunks[weg_chunk_quads[i]];
		const weg_section_t *s = &weg_sections[ch->section];
		int far = s->ground_count > 10 ? 10 : s->ground_count - 1;
		ch->visible = 0;
		if (!in_wedge(s->point) && !in_wedge(s->ground_right[far]) &&
			!in_wedge(s->ground_left[far]))
			continue;
		if (in_box(s->point) || in_box(s->edge_right) || in_box(s->edge_left) ||
			in_box(s->ground_right[far]) || in_box(s->ground_left[far])) {
			ch->visible = 1;
			if (first < 0)
				first = i;
			last = i;
		}
	}
	set_visible_weg_chunks(first, last + 1);
}
