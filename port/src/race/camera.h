/* The camera (camera.cpp, and mycontro.cpp/opptools.cpp's camera modes). */
#ifndef A2_CAMERA_H
#define A2_CAMERA_H

/* set_camera_position's modes. */
enum {
	CAMERA_CHASE = 1,    /* behind the car (F2) */
	CAMERA_COCKPIT = 2,  /* from the driver's seat (F1) */
	CAMERA_OPPONENT = 3, /* behind watched_opponent */
	CAMERA_OVERHEAD = 4, /* high above (a hidden key combination) */
};

extern float camera_height, camera_distance;
extern int camera_position;
extern int watched_opponent;
/* The player's car follows the road's slope: its pitch, yaw and roll for the matrix. */
extern int car_pitch, car_yaw, car_roll;
extern int look_offset; /* DAT_0045b274: always 0 */

void init_cameras(void);
/* Places the view: position (already negated), angles 0..1023. */
void set_camera(int camera, float x, float y, float z, int pitch, int yaw, int roll);
void set_camera_position(int mode);
/* Moves the camera after the player (update_camera). */
void update_camera(void);
/* The swoop down at the start (start_camera_intro). */
void start_camera_intro(void);

#endif
