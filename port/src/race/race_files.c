#include "race/race_files.h"

#include <stdio.h>

#include "res/archive.h"

static named_archive rcs;
static const char *disc_dir;

int race_files_open(const char *data_dir)
{
	disc_dir = data_dir;
	if (named_archive_open(&rcs, data_dir, "RCS")) {
		fprintf(stderr, "cannot read RCS archive in %s\n", data_dir);
		return -1;
	}
	return 0;
}

void race_files_close(void)
{
	named_archive_close(&rcs);
}

const uint8_t *race_file(const char *name, size_t *size)
{
	return named_archive_find(&rcs, name, size);
}

uint8_t *race_disc_file(const char *name, size_t *size)
{
	return read_file_ci(disc_dir, name, size);
}
