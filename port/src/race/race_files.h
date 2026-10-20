/* The race's files: RCS.IND/RCS.IMG (textures, models, sounds), GAMESPR (sprites) and
 * the track files on the disc (fileio.cpp's xvga_src_fopen in the original). */
#ifndef A2_RACE_FILES_H
#define A2_RACE_FILES_H

#include <stddef.h>
#include <stdint.h>

int race_files_open(const char *data_dir);
void race_files_close(void);
/* A file from RCS by name, or NULL. */
const uint8_t *race_file(const char *name, size_t *size);
/* A whole file from the disc directory (the tracks); caller frees. */
uint8_t *race_disc_file(const char *name, size_t *size);

#endif
