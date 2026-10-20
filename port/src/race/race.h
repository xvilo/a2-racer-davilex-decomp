/* The race's shared state (main.cpp, objio.cpp, mycontro.cpp, myinit.cpp). Names follow
 * the recovered symbols. */
#ifndef A2_RACE_H
#define A2_RACE_H

/* Race states (race_state; set_race_state). */
enum {
	RACE_RUNNING = 0,
	RACE_INTRO = 1,       /* the camera comes down from above */
	RACE_COUNTDOWN = 2,   /* 3, 2, 1, go */
	RACE_FINISHED = 3,    /* the player crossed the line */
	RACE_FINE = 4,        /* caught by the police: the fine is shown */
	RACE_OUT_OF_TIME = 5,
	RACE_NO_MONEY = 6,
	RACE_OUT_OF_FUEL = 7,
	RACE_EXIT = 8,
	RACE_PAUSED = 9,
};

/* track_info in A2SPEL.DAT, per stage. */
typedef struct {
	int music;
	const char *track;
	int direction;  /* 1: from the track's first point to its last; 0: back */
	int time_limit; /* seconds (TRACK/START1..3 in A2.INI replace them) */
} track_info_t;

extern track_info_t track_info[6];
extern int track_index;

extern float player_x, player_y, player_z;
extern float player_speed;
extern int player_chunk;
extern int player_heading;
extern int player_index;      /* the player's object */
extern int player_start_lane; /* the chunk the player was on (fuel use, wrong-way) */

extern int race_state;
extern int race_running;
extern int controls_enabled;
extern int race_finished_ok;
extern int screen_resolution; /* 0: 640x480, 1: 320x200 */
extern int dashboard_on_screen;
extern int horizon_on;
extern int detail_level;
extern int police_count, civilian_count;
extern int money, fuel, time_left;

/* Fuel per chunk driven, money for fines; running out ends the race (objio.cpp). */
void consume_fuel(int amount);
void pay_money(int amount);
int set_race_state(int state);

/* setracer.cpp: for each of the six racers (player first) its place, 1..6. */
const int *compute_race_ranking(void);
/* A racer's place now (1..6). */
int get_racer_entry(int racer);
void set_racer_finish_time(int racer);
/* Wall-clock seconds, as the original's time(). */
int race_time_now(void);

#endif
