/* Scenery (statobj.cpp, mkstobj.cpp): buildings, signs, trees, the sky and the
 * position boards, each one of 48 static object types with its model and texture. */
#ifndef A2_STATOBJ_H
#define A2_STATOBJ_H

#define STATIC_OBJECT_TYPES 48

void load_static_object_textures(void);
/* A new object of the type, its model and texture loaded (shared by every object of
 * the type), with an identity matrix; bonus.cpp and pion.cpp repeat this code. */
int new_static_object(int type);
/* A free-standing object of a type at (x, z), facing `heading` (the sky: type 0). */
int load_static_object(int type, float x, float z, int heading);
/* An object beside the road: at chunk, centred (position 0), on the road's left or
 * right `offset` out (1, 2), or on the ground beyond (3, 4); `angle` 0 or 1 picks
 * whether it faces along or across the road. */
void place_static_object_on_road(int type, int chunk, int position, int angle, int offset);
/* ETAPPE1..3.OMP: the stage's scenery. */
void init_static_objects(int stage);

#endif
