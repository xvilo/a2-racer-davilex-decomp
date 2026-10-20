/* Pionnen (pion.cpp): the traffic cones of the road works, knocked flying by the
 * player, the rivals and the police. */
#ifndef A2_PION_H
#define A2_PION_H

int alloc_pionnen(int count);
void free_pionnen(void);
/* 60 pionnen (init_pionnen). */
void init_pionnen(void);
void place_pion_left(int chunk, int lane);
void place_pion_right(int chunk, int lane);
/* A sign and ten cones on the outer lanes from the chunk on. */
void place_pion_group(int chunk);
/* setracer.cpp: four or five groups along the road. */
void place_random_pionnen(void);
/* Flying cones fall; cars hit the ones in their chunk and lane. */
void collide_with_pionnen(int opponent);
void update_pionnen(void);

#endif
