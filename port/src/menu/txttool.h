/* Text helpers of the menu (txttool.cpp). */
#ifndef A2_TXTTOOL_H
#define A2_TXTTOOL_H

/* Prints formatted text, shortened with "..." until it is narrower than max_width. */
void win_bprintf(int which, int x, int y, int max_width, const char *fmt, ...);
/* Prints formatted text word-wrapped between x and x + width, from y down. */
void win_printwrapped(int which, int x, int y, int width, const char *fmt, ...);

#endif
