/* Joysticks (joystick.cpp) and keyispressed_1 (key.cpp). Up to two joysticks (1, 2),
 * each turning its stick and first four buttons into eight "keys": left, right, up,
 * down, button 1..4, each mapped to a virtual-key code the game asks about. */
#ifndef A2_JOYSTICK_H
#define A2_JOYSTICK_H

/* Bit 0: joystick 1 attached, bit 1: joystick 2. */
int get_attached_joysticks(void);
/* Takes each joystick's present position as its centre. */
void calibrate_joysticks(void);
/* Reads the joysticks into their key states. */
void handle_joystick(void);
/* Joystick 1 or 2, slot 0..7 (left, right, up, down, buttons 1..4), the key it stands for. */
void set_joystick_keys(int joystick, int slot, int key);
/* The dead zone, a fraction (0.05..1) of the centre value. */
void set_joystick_sensibility(double sensibility);
/* The stick scaled to 0..width and 0..height (centre in the dead zone), and in
 * *buttons: 1 up, 2 down, 4 left, 8 right; 0 when there is no joystick. */
int get_joystick_position(int joystick, int width, int height, int *x, int *y, int *buttons);
/* Whether a joystick key standing for the key is down. */
int check_joystick_keys(int key);
/* The key on a device: bit 0 the keyboard, bit 1 the joysticks. */
int keyispressed_1(int key, int device);

#endif
