/* Save files (fileio.cpp's file_exist/file_read/file_write/file_delete).
 *
 * The original kept OPTIE.A2R, BEDIEN.A2R, SLOT.A2R, the score files and INFIO.INI
 * in the game directory. The port keeps the same files, byte for byte, in the user's
 * save directory (SDL_GetPrefPath), so saves from the original can be copied over. */
#ifndef A2_FILEIO_H
#define A2_FILEIO_H

#include <stddef.h>

/* Sets the save directory; NULL picks the platform default. */
void fileio_init(const char *save_dir);
const char *fileio_save_dir(void);
/* Full path of a save file, in a static buffer. */
const char *save_path(const char *name);

int file_exist(const char *name);
/* Reads exactly `size` bytes; returns 0 on success (the original returned nonzero for
 * a missing or short file, after which the caller deletes it and starts over). */
int file_read(void *buf, size_t size, const char *name);
int file_write(const void *buf, size_t size, const char *name);
void file_delete(const char *name);

#endif
