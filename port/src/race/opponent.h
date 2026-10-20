/* Everything that moves or collides on the road (opponent.cpp): the player, the rivals,
 * the police and the traffic, and the scenery objects' collision boxes. One opponent_t
 * per object; the layout follows the original's 0x100-byte entries. */
#ifndef A2_OPPONENT_H
#define A2_OPPONENT_H

typedef struct {
	int car_type;          /* index into the car type table */
	float x, y, z;
	float speed;
	float acceleration;
	float field_18;
	int field_1c;
	float field_20;
	int heading;           /* 0..1023 */
	int steer_angle;
	int steer_rate;        /* added to steer_angle each frame */
	float mass;            /* weights the collision response */
	float distance_to_target;
	int field_38[4];
	int field_48;
	float max_speed;
	int car_class;         /* copied from the car type table */
	float radius;          /* collision radius */
	float world_corners[12]; /* 4 bounding-box corners (x, y, z), rotated by heading */
	float local_corners[12]; /* the same corners relative to the object */
	float target_x, target_y, target_z;
	int chunk;             /* road chunk the car is on */
	int lane;
	int target_lane;
	int prev_waypoint;
	int waypoint;
	int direction;         /* 0 or 1, driving direction along the road */
	int object;            /* index into objects3d */
	int field_e0;
	int field_e4;
	int active;
	float push_x, push_z;  /* collision push */
	float offset_x, offset_z;
	int heading_delta;
} opponent_t;

typedef char opponent_size_check[sizeof(opponent_t) == 0x100 ? 1 : -1];

extern opponent_t *opponent_data;
extern int opponent_count;

int init_opponents(int count);
void exit_opponents(void);
int alloc_opponent_slot(void);

/* The four corners of the object's box, turned to its heading (update_racer_corners). */
void update_racer_corners(int opponent);
/* Keeps a car between the median and the road's edge; 1 if it hit one. */
int collide_racer_with_road_edge(int opponent);
/* Stops the player at the track's ends. */
void collide_player_with_track_ends(void);
/* The lane (0..10) a car is in, or -1 off the road. */
int get_racer_lane(int opponent);
extern int police_max_speed, racer_speed;

/* A car of a type (index into car_types) for a kind of driver; returns its object. */
int create_opponent(int car_type, int kind);
void place_opponent(int opponent, int direction, int chunk, int lane, float along);
void scatter_traffic(int first, int margin);
void init_active_opponent_list(int size);
void mark_active_racers(void);
/* Moves every car one frame; the player first (update_racers). */
void update_racers(void);

/* objio.cpp: the object's bounding rectangle and radius into its opponent entry. */
void compute_object_bounds(int object);

#endif
