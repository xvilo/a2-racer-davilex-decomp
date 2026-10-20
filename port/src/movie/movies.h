/* Smacker movies (the original's A2MOVIE.DAT). */
#ifndef A2_MOVIES_H
#define A2_MOVIES_H

/* Plays a movie from the disc; returns 1 when the player stopped it, 0 when it ran to
 * the end, -1 when it cannot be played. */
int play_movie(const char *data_dir, const char *name);

#endif
