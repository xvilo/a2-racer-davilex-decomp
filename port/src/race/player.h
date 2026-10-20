/* The player's car: its controls, gearbox and steering (mycontro.cpp, opptools.cpp),
 * and its object (myinit.cpp). Also the car type table every car comes from. */
#ifndef A2_PLAYER_H
#define A2_PLAYER_H

/* car_types in A2SPEL.DAT. */
typedef struct {
	const char *mesh;
	const char *texture_hi; /* unused */
	const char *texture;
	int texture_id;
	int mesh_id;
	float max_speed;
	int car_class;
	float mass;
} car_type_t;

#define CAR_TYPE_COUNT 23
#define PLAYER_CAR_TYPE 19 /* the red BMW */

extern car_type_t car_types[CAR_TYPE_COUNT];
extern int engine_rpm;

void load_car_textures(void);
/* Keys and the gearbox table from INFIO.INI's settings. */
void init_controls(void);
/* Creates the player's car on a chunk and lane, `offset` along the road. */
void load_player_object(int direction, int chunk, int lane, float offset);
/* The player's steering, throttle and brake for this frame. */
void handle_player_controls(void);
/* The gearbox step for a speed (after collisions changed it). */
void select_rpm_step_for_speed(float speed);
void halt_player_car(void);
void stop_player_car(void);

#endif
