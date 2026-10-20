/* Reading the game straight from its CD image: a raw .bin (2352-byte sectors, Mode 1
 * or Mode 2 XA) or an .iso (2048-byte sectors). The files in the disc's root are
 * unpacked once into a folder, which then serves as the data folder. */
#ifndef A2_DISCIMAGE_H
#define A2_DISCIMAGE_H

/* Whether the path names a CD image (by its extension). */
int is_disc_image(const char *path);
/* Unpacks the root files of the image into out_dir (created if needed); 0 on success.
 * Does nothing when out_dir already holds the game. */
int disc_image_unpack(const char *image, const char *out_dir);

#endif
