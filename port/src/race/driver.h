/* Who drives the other cars (driver.cpp). Each car has a driver that picks its lane
 * and speed every frame from what is on the road map ahead and behind:
 *   civilians keep to a lane and overtake slower cars;
 *   rivals take the lane with the most room, preferring their own lane;
 *   police park on the hard shoulder, chase a racer that speeds past (over 9),
 *   overtake and brake in front of it: catching the player means a fine. */
#ifndef A2_DRIVER_H
#define A2_DRIVER_H

enum { DRIVER_CIVILIAN = 2, DRIVER_RIVAL = 3, DRIVER_POLICE = 4 };

typedef struct {
	int opponent;   /* the car */
	int lane;       /* the lane the driver prefers */
	float speed;    /* the speed the driver prefers (an int in the original's float slot) */
	int assigned;
	int kind;       /* DRIVER_* */
	int index;      /* the rival's or police car's own number */
	int used;
} driver_t;

/* A rival's state (driver_lane_data). */
typedef struct {
	int state;      /* 4: finished, slowing down to a stop */
	int stopped_by; /* the police car that stopped it, -1 */
	int counter;    /* frames left slowed down, or the speed after the finish */
} rival_state_t;

/* A police car's state (driver_path_data). */
typedef struct {
	int state;      /* 0 parked, 1 chasing, 2 going back */
	int target;     /* the racer chased */
	int fast;       /* the racer seen was speeding */
	int counter;
	int speed;      /* chase speed */
	int lane;
} police_state_t;

extern driver_t *driver_list;
extern rival_state_t *rival_states;
extern float racer_high_speed, racer_low_speed;
extern int racer_range;

void init_drivers(int count, int rivals, int police);
void exit_drivers(void);
int get_new_driver(void);
void assign_driver_to_opponent(int driver, int opponent, int kind);
void drive_opponent(int driver);

/* The road map: which car is on which chunk and lane (driver.cpp/opponent.cpp). */
void init_opponent_map(void);
void exit_opponent_map(void);
void set_map_occupant(int opponent, int chunk, int lane);
int get_map_occupant(int chunk, int lane);
void clear_map_occupant(int chunk, int lane);

#endif
