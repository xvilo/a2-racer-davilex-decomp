#include "race/statobj.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "race/object3d.h"
#include "race/opponent.h"
#include "race/race.h"
#include "race/race_files.h"
#include "race/render.h"
#include "race/texture.h"
#include "race/weg.h"

/* static_object_types in A2SPEL.DAT: model, texture, then the loaded ids. */
static struct {
	const char *mesh_name;
	const char *texture_name;
	int texture;
	int mesh; /* frame index once loaded */
} static_object_types[STATIC_OBJECT_TYPES] = {
	{ "sky.d3d", "sky.pcx", -1, -1 }, { "bovw.d3d", "50.pcx", -1, -1 }, { "arena.d3d", "arena.pcx", -1, -1 },
	{ "viaduct.d3d", "viaduct.pcx", -1, -1 }, { "molent.d3d", "molen.pcx", -1, -1 },
	{ "shell.d3d", "shell.pcx", -1, -1 }, { "roadsgn.d3d", "roadsgn.pcx", -1, -1 }, { "uit.d3d", "uit.pcx", -1, -1 },
	{ "zaltboml.d3d", "zaltboml.pcx", -1, -1 }, { "lekbrug.d3d", "lekbrug.pcx", -1, -1 },
	{ "gebouwg.d3d", "gebouw.pcx", -1, -1 }, { "gebouw2g.d3d", "tile.pcx", -1, -1 },
	{ "gebouw3g.d3d", "tile2.pcx", -1, -1 }, { "trees.d3d", "trees.pcx", -1, -1 },
	{ "wegresg2.d3d", "wegresg2.pcx", -1, -1 }, { "elek4.d3d", "elek4.pcx", -1, -1 },
	{ "huisje.d3d", "huisje.pcx", -1, -1 }, { "rijtjes.d3d", "rijtjes.pcx", -1, -1 },
	{ "werkweg.d3d", "werkweg.pcx", -1, -1 }, { "werkweg2.d3d", "werkweg2.pcx", -1, -1 },
	{ "flits.d3d", "flits.pcx", -1, -1 }, { "praatp.d3d", "praatp.pcx", -1, -1 }, { "start.d3d", "start.pcx", -1, -1 },
	{ "finish.d3d", "finish.pcx", -1, -1 }, { "geld.d3d", "geld.pcx", -1, -1 },
	{ "benzine.d3d", "benzine.pcx", -1, -1 }, { "tijd.d3d", "tijd.pcx", -1, -1 },
	{ "lantaarn.d3d", "lantaarn.pcx", -1, -1 }, { "eindh.d3d", "eindh.pcx", -1, -1 },
	{ "amsterd.d3d", "amsterd.pcx", -1, -1 }, { "denbosch.d3d", "denbosch.pcx", -1, -1 },
	{ "maastr.d3d", "maastr.pcx", -1, -1 }, { "utrecht.d3d", "utrecht.pcx", -1, -1 },
	{ "s_maastr.d3d", "s_maastr.pcx", -1, -1 }, { "s_utr.d3d", "s_utr.pcx", -1, -1 },
	{ "s_denb.d3d", "s_denb.pcx", -1, -1 }, { "s_adam.d3d", "s_adam.pcx", -1, -1 },
	{ "s_eindho.d3d", "s_eindho.pcx", -1, -1 }, { "number1.d3d", "number1.pcx", -1, -1 },
	{ "number2.d3d", "number2.pcx", -1, -1 }, { "number3.d3d", "number3.pcx", -1, -1 },
	{ "number4.d3d", "number4.pcx", -1, -1 }, { "number5.d3d", "number5.pcx", -1, -1 },
	{ "number6.d3d", "number6.pcx", -1, -1 }, { "hek.d3d", "hek.pcx", -1, -1 }, { "koe.d3d", "koe.pcx", -1, -1 },
	{ "100.d3d", "100.pcx", -1, -1 }, { "babe.d3d", "babe.pcx", -1, -1 },
};

/* ETAPPE1..3.OMP per stage; stages 4..6 drive the tracks back. */
static const char *const omp_files[6] = {
	"ETAPPE1.OMP", "ETAPPE2.OMP", "ETAPPE3.OMP", "ETAPPE3.OMP", "ETAPPE2.OMP", "ETAPPE1.OMP",
};

void load_static_object_textures(void)
{
	for (int i = 0; i < STATIC_OBJECT_TYPES; i++) {
		static_object_types[i].texture = load_texture(static_object_types[i].texture_name, 0);
		static_object_types[i].mesh = -1;
	}
}

/* The type's model, loaded once and shared by every object of the type. */
static int new_static_object(int type)
{
	int object = get_new_object3d();
	object3d_t *o = &objects3d[object];
	o->in_use = 1;
	o->anim = init_anim3d(1);
	o->frame = 0;
	o->update_interval = 1;
	o->field_36 = 0;
	if (static_object_types[type].mesh == -1) {
		load_object_mesh(object, static_object_types[type].mesh_name);
		static_object_types[type].mesh = anims3d[o->anim].frames[o->frame];
	}
	else {
		anims3d[o->anim].frames[o->frame] = static_object_types[type].mesh;
	}
	if (static_object_types[type].texture == -1) {
		fprintf(stderr, "texture not loaded for static object %d\n", type);
		exit(1);
	}
	o->texture = static_object_types[type].texture;
	matrix_t identity;
	matrix_identity(&identity);
	set_object3d_matrix(object, &identity);
	return object;
}

/* Places the object once, turned to its heading (field_3f), then stops updating. */
static int update_static_object(int object)
{
	object3d_t *o = &objects3d[object];
	matrix_t m = o->matrix;
	m.m[12] = o->x;
	m.m[13] = o->y;
	m.m[14] = o->z;
	m.m[15] = 1.0f;
	rotate_matrix_y(&m, 0x4ff - (o->field_3f & 0x3ff));
	o->matrix = m;
	o->update_interval = -1;
	return 0;
}

/* The sky dome stays centred on the player. */
static int update_sky_object(int object)
{
	object3d_t *o = &objects3d[object];
	o->matrix.m[12] = player_x;
	o->matrix.m[14] = player_z;
	o->matrix.m[15] = 1.0f;
	o->matrix.m[13] = 0.0f;
	o->visible = 1;
	o->update_interval = 1;
	return 0;
}

/* The boards "1".."6" float above the racer in that place (types 0x26..0x2b). */
static int update_position_board(int object, int place)
{
	const int *ranking = compute_race_ranking();
	int i = 0;
	if (!ranking)
		return 0;
	while (i < 6 && ranking[i] != place)
		i++;
	if (i == 6)
		return 0;
	object3d_t *o = &objects3d[object];
	matrix_t m;
	matrix_identity(&m);
	m.m[12] = opponent_data[i].x;
	m.m[13] = opponent_data[i].y + 10.0f;
	m.m[14] = opponent_data[i].z;
	rotate_matrix_y(&m, 0x3ff - (opponent_data[i].heading & 0x3ff));
	o->matrix = m;
	o->visible = 1;
	o->update_interval = 1;
	return 0;
}

static int update_static_object_26(int object) { return update_position_board(object, 1); }
static int update_static_object_27(int object) { return update_position_board(object, 2); }
static int update_static_object_28(int object) { return update_position_board(object, 3); }
static int update_static_object_29(int object) { return update_position_board(object, 4); }
static int update_static_object_2a(int object) { return update_position_board(object, 5); }
static int update_static_object_2b(int object) { return update_position_board(object, 6); }

int load_static_object(int type, float x, float z, int heading)
{
	static int (*const boards[6])(int) = {
		update_static_object_26, update_static_object_27, update_static_object_28,
		update_static_object_29, update_static_object_2a, update_static_object_2b,
	};
	int object = new_static_object(type);
	object3d_t *o = &objects3d[object];
	if (type == 0)
		o->update = update_sky_object;
	else if (type >= 0x26 && type <= 0x2b)
		o->update = boards[type - 0x26];
	else
		o->update = update_static_object;
	o->update_interval = 1;
	o->x = x;
	o->y = 0.0f;
	o->z = z;
	o->field_3f = heading;
	o->in_use = 1;
	build_object_execute_buffer(object);
	return object;
}

static int angle_of(double y, double x)
{
	return (int)((float)atan2(y, x) * 1024.0 / 6.283185307);
}

void place_static_object_on_road(int type, int chunk, int position, int angle, int offset)
{
	int object = new_static_object(type);
	object3d_t *o = &objects3d[object];
	const vertex3d_t *p = &vertices3d[weg_sections[weg_chunks[chunk].section].point];
	o->opponent = alloc_opponent_slot();
	o->update = update_static_object;
	compute_object_bounds(object);
	float side[3] = { weg_chunks[chunk].side[0], 0.0f, weg_chunks[chunk].side[2] };
	normalize_vector(side);
	float d = (float)offset, radius = opponent_data[o->opponent].radius;
	switch (position) {
	case 0:
		/* The original reads an uninitialised x here; the centre point it is. */
		o->x = p->x;
		o->y = road_height_at(chunk, o->x, p->z);
		o->z = p->z;
		break;
	case 1:
		o->x = p->x - side[0] * d;
		o->z = p->z - side[2] * d;
		o->y = road_height_at(chunk, o->x, o->z);
		break;
	case 2:
		o->x = side[0] * d + p->x;
		o->z = side[2] * d + p->z;
		o->y = road_height_at(chunk, o->x, o->z);
		break;
	case 3:
		o->x = p->x - (radius + d * 2.0f) * side[0];
		o->y = 0.0f;
		o->z = p->z - (d * 2.0f + radius) * side[2];
		break;
	case 4:
		o->x = (radius + d * 2.0f) * side[0] + p->x;
		o->y = 0.0f;
		o->z = (d * 2.0f + radius) * side[2] + p->z;
		break;
	}
	/* Facing: along the road (angle 0) or across it (1), turned to the road's side. */
	int left = position == 1 || position == 3;
	if (angle == 0) {
		if (!left)
			o->field_3f = side[0] == 0.0f ? 0x200 : angle_of(-side[2], -side[0]);
		else
			o->field_3f = side[0] == 0.0f ? 0 : angle_of(side[2], side[0]);
	}
	else if (angle == 1) {
		if (!left)
			o->field_3f = side[2] == 0.0f ? 0x100 : angle_of(-side[0], side[2]);
		else
			o->field_3f = side[2] == 0.0f ? 0x312 : angle_of(side[0], -side[2]);
	}
	opponent_data[o->opponent].active = 0;
	o->in_use = 1;
	build_object_execute_buffer(object);
}

void init_static_objects(int stage)
{
	size_t size;
	if (stage < 0 || stage > 5) {
		fprintf(stderr, "illegal level %d\n", stage);
		exit(1);
	}
	uint8_t *data = race_disc_file(omp_files[stage], &size);
	if (!data || size < 4) {
		fprintf(stderr, "could not open %s\n", omp_files[stage]);
		exit(1);
	}
	int32_t count;
	memcpy(&count, data, 4);
	for (int i = 0; i < count && 4 + (size_t)(i + 1) * 20 <= size; i++) {
		const uint8_t *r = data + 4 + i * 20;
		int32_t chunk, angle, position, id;
		float offset;
		memcpy(&chunk, r, 4);
		memcpy(&angle, r + 4, 4);
		memcpy(&position, r + 8, 4);
		memcpy(&id, r + 12, 4);
		memcpy(&offset, r + 16, 4);
		if (id >= 0 && id < STATIC_OBJECT_TYPES && chunk >= 0 && position >= 0 && angle >= 0 &&
			chunk < weg_chunk_count)
			place_static_object_on_road(id, chunk, position, angle, (int)offset);
	}
	free(data);
}
