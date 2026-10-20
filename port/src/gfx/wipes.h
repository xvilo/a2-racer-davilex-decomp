/* Screen transitions (wipes.cpp and its wipe*.cpp files): bring the picture in a
 * back or buffer screen onto the front screen. */
#ifndef A2_WIPES_H
#define A2_WIPES_H

enum {
	WIPE_VROLLBAR_UP = 1,
	WIPE_VROLLBAR_DOWN = 2,
	WIPE_PIXELFADE = 3,
	WIPE_SCROLL_LEFT = 4,
	WIPE_SCROLL_TOP = 5,
	WIPE_SCROLL_RIGHT = 6,
	WIPE_SCROLL_BOTTOM = 7,
	WIPE_LUXAFLEX_HORIZONTAL = 8,
	WIPE_LUXAFLEX_VERTICAL = 9,
};

void handle_wipe(int src, int type, int delay);
void handle_wipe_1(int src, int type, int x, int y, int w, int h, int delay);

#endif
