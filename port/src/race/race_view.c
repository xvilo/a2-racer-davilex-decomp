#include "race/race_view.h"

#include <stdio.h>

#include "platform/gl.h"
#include "platform/sys.h"
#include "race/math3d.h"
#include "race/object3d.h"
#include "race/opponent.h"
#include "race/race.h"
#include "race/race_files.h"
#include "race/render.h"
#include "race/statobj.h"
#include "race/texture.h"
#include "race/weg.h"


static void place_camera(float x, float y, float z, int yaw, int pitch)
{
	matrix_t r, t;
	matrix_identity(&r);
	rotate_matrix_x(&r, pitch);
	rotate_matrix_z(&r, 0);
	rotate_matrix_y(&r, yaw);
	matrix_translation(&t, -x, -y, -z);
	set_view(&r, &t);
}

int race_view(game_t *g, int stage)
{
	if (!sys_gl()) {
		fprintf(stderr, "race view: no OpenGL\n");
		return 1;
	}
	if (stage < 0 || stage > 5 || race_files_open(g->data_dir))
		return 1;
	init_math3d();
	alloc_3d_arrays(2000, 4000, 200000, 100000);
	alloc_objects3d(800);
	alloc_textures(0x50);
	change_viewport_size(0, 0, 640, 480);
	set_background("back.pcx");
	road_texture = load_texture("2baans.pcx", 0);
	grass_texture = load_texture("grass2.pcx", 0);
	if (load_weg_data(track_info[stage].track, 0x14))
		return 1;
	printf("race view: %s, %d chunks\n", track_info[stage].track, weg_chunk_count);
	init_opponents(600);
	load_static_object_textures();
	load_static_object(0, 0.0f, 0.0f, 0); /* the sky (with the horizon option on) */
	init_static_objects(stage);

	float pos = 0.0f; /* chunks travelled */
	uint32_t last = sys_ticks();
	sys_input in = { 0 };
	for (;;) {
		sys_poll(&in);
		if (in.quit || in.key == KEY_ESCAPE)
			break;
		uint32_t now = sys_ticks();
		pos += (now - last) * 0.004f;
		last = now;
		int c = (int)pos;
		if (c >= weg_chunk_count - 2) {
			pos = 0;
			c = 0;
		}
		const weg_section_t *s0 = &weg_sections[weg_chunks[c].section];
		const weg_section_t *s1 = &weg_sections[weg_chunks[c + 1].section];
		const vertex3d_t *a = &vertices3d[s0->point], *b = &vertices3d[s1->point];
		float f = pos - c;
		float x = a->x + (b->x - a->x) * f, y = a->y + (b->y - a->y) * f;
		float z = a->z + (b->z - a->z) * f;
		/* On the right-hand carriageway, a car's height above the road. */
		const float *side = weg_chunks[c].side;
		x += side[0] * 60.0f;
		z += side[2] * 60.0f;
		place_camera(x, y + 25.0f, z, s0->heading - 256, -20);
		player_x = x;
		player_z = z;
		player_chunk = c;
		check_objects3d_visibility((int)x - 4000, (int)z - 4000, (int)x + 4000, (int)z + 4000);
		set_visible_weg_chunks(c - 2, c + 60);
		sys_gl_begin_frame();
		render_3d_frame();
		sys_gl_swap();
	}
	exit_opponents();
	free_weg_data();
	free_textures();
	free_objects3d();
	free_3d_arrays();
	race_files_close();
	return 0;
}
