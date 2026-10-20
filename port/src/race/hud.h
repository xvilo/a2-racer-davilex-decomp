/* What is drawn over the 3D picture (dashbrd.cpp, radar.cpp, menuanim.cpp, and the
 * pictures objio.cpp's race states show): the dashboard or the small indicators, the
 * radar, the time, money and place, and the countdown and messages. The original drew
 * these 8-bit sprites from GAMESPR onto its back buffer; the port draws them onto an
 * 8-bit layer laid over the 3D frame. */
#ifndef A2_HUD_H
#define A2_HUD_H

#include "game.h"

extern int radar_on; /* F5 */

int init_hud(game_t *g);
void exit_hud(void);
/* Starts a frame's layer. */
void hud_begin(void);
void draw_radar(void);
void draw_dashboard(void);
/* A frame of one of the race's anims at (x, y); flags as set_mem_image's. */
void hud_image(int anim, int frame, int x, int y, int flags);
/* Lays the layer over the 3D frame. */
void hud_end(void);

#endif
