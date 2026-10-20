/* Resource archives: <name>.IND (index) + <name>.IMG (data).
 *
 * Named archives (RCS, A2MRCS) hold files by name: PCX pictures, WAV sounds and D3D
 * models. Sprite archives (A2MRC, GAMESPR) hold numbered 8-bit images with a center
 * point. The original reads them in fileio.cpp and mem_image code of the xvga library.
 */
#ifndef A2_ARCHIVE_H
#define A2_ARCHIVE_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
	char name[16];
	uint32_t offset;
	uint32_t size;
} archive_entry;

typedef struct {
	uint8_t *data; /* the whole .IMG file */
	size_t data_size;
	archive_entry *entries;
	int count;
} named_archive;

typedef struct {
	int width, height;
	int center_x, center_y;
	const uint8_t *pixels; /* width * height palette indices, inside the archive data */
} sprite;

typedef struct {
	uint8_t *data;
	size_t data_size;
	sprite *sprites;
	int count;
} sprite_archive;

/* Paths are "<dir>/<NAME>"; .IND and .IMG are appended. Return 0 on success. */
int named_archive_open(named_archive *a, const char *dir, const char *name);
void named_archive_close(named_archive *a);
/* Case-insensitive lookup; returns the entry's bytes or NULL. */
const uint8_t *named_archive_find(const named_archive *a, const char *file, size_t *size);

int sprite_archive_open(sprite_archive *a, const char *dir, const char *name);
void sprite_archive_close(sprite_archive *a);

/* Reads a whole file (case-insensitive name match inside dir). Caller frees. */
uint8_t *read_file_ci(const char *dir, const char *name, size_t *size);

#endif
