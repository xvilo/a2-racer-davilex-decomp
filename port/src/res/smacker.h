/* Smacker video ("SMK2"), the format of A2INTRO.SMK, A2WIN.SMK and A2EXIT.SMK. The
 * original played them with RAD's SMACKW32.DLL; this is an independent decoder for
 * the parts these files use: 8-bit video in 4x4 blocks with Huffman-coded block types,
 * colours and maps, palette updates, and Huffman/DPCM-compressed audio. */
#ifndef A2_SMACKER_H
#define A2_SMACKER_H

#include <stddef.h>
#include <stdint.h>

typedef struct smacker smacker;

/* Takes over nothing: `data` must stay valid while the file is open. */
smacker *smk_open(const uint8_t *data, size_t size);
void smk_close(smacker *s);

int smk_width(const smacker *s);
int smk_height(const smacker *s);
int smk_frame_count(const smacker *s);
/* Milliseconds per frame. */
double smk_frame_ms(const smacker *s);

/* Decodes the next frame; returns 0 on success, -1 at the end or on bad data. */
int smk_next_frame(smacker *s);
const uint8_t *smk_pixels(const smacker *s);   /* width * height palette indices */
const uint8_t *smk_palette(const smacker *s);  /* 256 RGB triples, 8 bits each */

/* The first audio track, decoded in full as signed 16-bit PCM. Returns the sample
 * count (per channel) and fills rate/channels; *out is malloc'd. 0 when silent. */
size_t smk_decode_audio(smacker *s, int16_t **out, int *rate, int *channels);

#endif
