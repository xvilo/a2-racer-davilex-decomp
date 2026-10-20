/* What is in view (mapexec.cpp, weg.cpp's compute_road_height): a box of the map
 * ahead of the camera and a wedge opening forward from just behind it. Objects inside
 * the box are drawn; road chunks inside both. */
#ifndef A2_MAPEXEC_H
#define A2_MAPEXEC_H

/* The wedge: lines p3 -> p2 and p2 -> p1 (p2 behind the camera). */
void set_view_quad(int x1, int z1, int x2, int z2, int x3, int z3);
/* The box around map position (x, z) in units of 30, placed ahead for the octant the
 * camera looks into (0..7); also hides objects outside it. */
void update_visible_tiles(int x, int z, int octant);
/* Marks the road chunks in view (get_road_height / compute_road_height). */
void get_road_height(void);
/* The octant of a direction (x, z) as the camera code picks it. */
int view_octant(float x, float z);

#endif
