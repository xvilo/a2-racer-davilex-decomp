#include "res/smacker.h"

#include <stdlib.h>
#include <string.h>

/* --- bit reader (least significant bit first) -------------------------------------- */

typedef struct {
	const uint8_t *p;
	size_t size;   /* bytes */
	size_t pos;    /* bits */
} bits;

static int get_bit(bits *b)
{
	if (b->pos >= b->size * 8) {
		b->pos++;
		return 0;
	}
	int v = b->p[b->pos >> 3] >> (b->pos & 7) & 1;
	b->pos++;
	return v;
}

static unsigned get_bits(bits *b, int n)
{
	unsigned v = 0;
	for (int i = 0; i < n; i++)
		v |= (unsigned)get_bit(b) << i;
	return v;
}

/* --- Huffman trees ----------------------------------------------------------------- */

/* Nodes: a leaf holds a value; a branch holds the index of its "1" child, its "0"
 * child following directly. */
typedef struct {
	int leaf;
	unsigned value;   /* leaf value */
	int one;          /* branch: index of the child for bit 1 */
} node;

typedef struct {
	node *n;
	int count, cap;
} tree;

static int tree_add(tree *t)
{
	if (t->count == t->cap) {
		t->cap = t->cap ? t->cap * 2 : 64;
		t->n = realloc(t->n, sizeof *t->n * (size_t)t->cap);
	}
	memset(&t->n[t->count], 0, sizeof t->n[0]);
	return t->count++;
}

static int build_tree8(tree *t, bits *b, int depth)
{
	int i = tree_add(t);
	if (depth > 32 || !get_bit(b)) {
		t->n[i].leaf = 1;
		t->n[i].value = get_bits(b, 8);
		return i;
	}
	build_tree8(t, b, depth + 1);
	int one = build_tree8(t, b, depth + 1);
	t->n[i].one = one;
	return i;
}

static unsigned tree_decode(const tree *t, bits *b)
{
	if (!t->count)
		return 0;
	int i = 0;
	while (!t->n[i].leaf) {
		i = get_bit(b) ? t->n[i].one : i + 1;
		if (i >= t->count)
			return 0;
	}
	return t->n[i].value;
}

/* An 8-bit tree as stored in the file: a presence bit, the tree, a closing bit. */
static void read_tree8(tree *t, bits *b)
{
	t->count = 0;
	if (get_bit(b)) {
		build_tree8(t, b, 0);
		get_bit(b);
	}
}

/* The 16-bit trees of the video: leaves are a low and a high byte from two 8-bit
 * trees; three leaves are "recent value" slots, reset each frame. */
typedef struct {
	tree t;
	int cache_leaf[3];   /* node indices of the three escape leaves */
	unsigned cache[3];
} bigtree;

static int build_big(bigtree *bt, bits *b, const tree *lo, const tree *hi, const unsigned esc[3],
	int depth)
{
	int i = tree_add(&bt->t);
	if (depth > 64 || !get_bit(b)) {
		unsigned v = tree_decode(lo, b) | tree_decode(hi, b) << 8;
		bt->t.n[i].leaf = 1;
		bt->t.n[i].value = v;
		for (int k = 0; k < 3; k++) {
			if (v == esc[k] && bt->cache_leaf[k] < 0) {
				bt->cache_leaf[k] = i;
				break;
			}
		}
		return i;
	}
	build_big(bt, b, lo, hi, esc, depth + 1);
	int one = build_big(bt, b, lo, hi, esc, depth + 1);
	bt->t.n[i].one = one;
	return i;
}

static void read_big(bigtree *bt, bits *b)
{
	bt->t.count = 0;
	bt->cache_leaf[0] = bt->cache_leaf[1] = bt->cache_leaf[2] = -1;
	if (!get_bit(b))
		return; /* absent: every code is 0 */
	tree lo = { 0 }, hi = { 0 };
	read_tree8(&lo, b);
	read_tree8(&hi, b);
	unsigned esc[3];
	for (int k = 0; k < 3; k++)
		esc[k] = get_bits(b, 16);
	build_big(bt, b, &lo, &hi, esc, 0);
	get_bit(b);
	free(lo.n);
	free(hi.n);
}

static void big_reset(bigtree *bt)
{
	bt->cache[0] = bt->cache[1] = bt->cache[2] = 0;
}

static unsigned big_decode(bigtree *bt, bits *b)
{
	if (!bt->t.count)
		return 0;
	int i = 0;
	while (!bt->t.n[i].leaf) {
		i = get_bit(b) ? bt->t.n[i].one : i + 1;
		if (i >= bt->t.count)
			return 0;
	}
	unsigned v = bt->t.n[i].value;
	for (int k = 0; k < 3; k++) {
		if (i == bt->cache_leaf[k])
			v = bt->cache[k];
	}
	if (v != bt->cache[0]) {
		bt->cache[2] = bt->cache[1];
		bt->cache[1] = bt->cache[0];
		bt->cache[0] = v;
	}
	return v;
}

/* --- file --------------------------------------------------------------------------- */

struct smacker {
	const uint8_t *data;
	size_t size;
	int width, height, frames;
	double frame_ms;
	uint32_t flags;
	uint32_t audio_rate[7];
	const uint32_t *frame_sizes_le;
	const uint8_t *frame_types;
	size_t first_frame;      /* offset of frame 0 */
	bigtree mmap, mclr, full, type;
	int current;             /* next frame to decode */
	size_t offset;           /* its offset */
	uint8_t *pixels;
	uint8_t palette[768];
};

static uint32_t rd32(const uint8_t *p)
{
	return (uint32_t)(p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24);
}

smacker *smk_open(const uint8_t *data, size_t size)
{
	if (size < 104 || memcmp(data, "SMK2", 4))
		return NULL;
	smacker *s = calloc(1, sizeof *s);
	s->data = data;
	s->size = size;
	s->width = (int)rd32(data + 4);
	s->height = (int)rd32(data + 8);
	s->frames = (int)rd32(data + 12);
	int32_t rate = (int32_t)rd32(data + 16);
	s->frame_ms = rate > 0 ? rate : rate < 0 ? -rate / 100.0 : 100.0;
	s->flags = rd32(data + 20);
	for (int i = 0; i < 7; i++)
		s->audio_rate[i] = rd32(data + 72 + 4 * i);
	uint32_t trees_size = rd32(data + 52);
	int ring = s->flags & 1;
	size_t pos = 104;
	size_t table = (size_t)(s->frames + ring) * 4 + (size_t)(s->frames + ring);
	if (pos + table + trees_size > size || s->width <= 0 || s->height <= 0) {
		free(s);
		return NULL;
	}
	s->frame_sizes_le = (const uint32_t *)(data + pos);
	s->frame_types = data + pos + (size_t)(s->frames + ring) * 4;
	pos += table;
	bits b = { data + pos, trees_size, 0 };
	read_big(&s->mmap, &b);
	read_big(&s->mclr, &b);
	read_big(&s->full, &b);
	read_big(&s->type, &b);
	s->first_frame = s->offset = pos + trees_size;
	s->pixels = calloc((size_t)s->width * s->height, 1);
	return s;
}

void smk_close(smacker *s)
{
	if (!s)
		return;
	free(s->mmap.t.n);
	free(s->mclr.t.n);
	free(s->full.t.n);
	free(s->type.t.n);
	free(s->pixels);
	free(s);
}

int smk_width(const smacker *s) { return s->width; }
int smk_height(const smacker *s) { return s->height; }
int smk_frame_count(const smacker *s) { return s->frames; }
double smk_frame_ms(const smacker *s) { return s->frame_ms; }
const uint8_t *smk_pixels(const smacker *s) { return s->pixels; }
const uint8_t *smk_palette(const smacker *s) { return s->palette; }

static uint32_t frame_size(const smacker *s, int i)
{
	return rd32((const uint8_t *)&s->frame_sizes_le[i]) & ~3u;
}

/* 6-bit palette values to 8 bits as the original player did it (set_smack_palette
 * shifts SMACKW32's 6-bit palette left by 2, so white is 252). */
static uint8_t pal8(int v)
{
	return (uint8_t)((v & 63) << 2);
}

static void decode_palette(smacker *s, const uint8_t *p, size_t len)
{
	uint8_t old[768];
	memcpy(old, s->palette, sizeof old);
	size_t q = 1;
	for (int i = 0; i < 256 && q < len;) {
		uint8_t c = p[q++];
		if (c & 0x80) {
			i += (c & 0x7f) + 1; /* keep these entries */
		}
		else if (c & 0x40) {
			int n = (c & 0x3f) + 1;
			int src = q < len ? p[q++] : 0;
			while (n-- && i < 256 && src < 256) {
				memcpy(&s->palette[i * 3], &old[src * 3], 3);
				i++;
				src++;
			}
		}
		else {
			if (q + 2 > len)
				break;
			s->palette[i * 3] = pal8(c);
			s->palette[i * 3 + 1] = pal8(p[q++]);
			s->palette[i * 3 + 2] = pal8(p[q++]);
			i++;
		}
	}
}

static const int block_runs[64] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
	25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46,
	47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 128, 256, 512, 1024, 2048,
};

static void decode_video(smacker *s, const uint8_t *p, size_t len)
{
	bits b = { p, len, 0 };
	int bw = s->width / 4, bh = s->height / 4, total = bw * bh, blk = 0;
	big_reset(&s->mmap);
	big_reset(&s->mclr);
	big_reset(&s->full);
	big_reset(&s->type);
	while (blk < total) {
		unsigned type = big_decode(&s->type, &b);
		int run = block_runs[(type >> 2) & 0x3f];
		for (; run > 0 && blk < total; run--, blk++) {
			uint8_t *out = s->pixels + (blk / bw) * 4 * s->width + (blk % bw) * 4;
			switch (type & 3) {
			case 0: { /* two colours and a 16-bit map */
				unsigned clr = big_decode(&s->mclr, &b);
				unsigned map = big_decode(&s->mmap, &b);
				uint8_t hi = (uint8_t)(clr >> 8), lo = (uint8_t)clr;
				for (int y = 0; y < 4; y++, out += s->width) {
					for (int x = 0; x < 4; x++, map >>= 1)
						out[x] = map & 1 ? hi : lo;
				}
				break;
			}
			case 1: /* full: eight 16-bit codes */
				for (int y = 0; y < 4; y++, out += s->width) {
					unsigned v = big_decode(&s->full, &b);
					out[2] = (uint8_t)v;
					out[3] = (uint8_t)(v >> 8);
					v = big_decode(&s->full, &b);
					out[0] = (uint8_t)v;
					out[1] = (uint8_t)(v >> 8);
				}
				break;
			case 2: /* unchanged */
				break;
			case 3: /* solid colour */
				for (int y = 0; y < 4; y++, out += s->width)
					memset(out, (int)(type >> 8), 4);
				break;
			}
		}
	}
}

int smk_next_frame(smacker *s)
{
	if (s->current >= s->frames)
		return -1;
	uint32_t size = frame_size(s, s->current);
	uint8_t types = s->frame_types[s->current];
	if (s->offset + size > s->size)
		return -1;
	const uint8_t *p = s->data + s->offset;
	size_t left = size;
	if (types & 1) {
		size_t len = (size_t)p[0] * 4;
		if (len > left)
			return -1;
		decode_palette(s, p, len);
		p += len;
		left -= len;
	}
	for (int t = 0; t < 7; t++) {
		if (types & (2 << t)) {
			if (left < 4)
				return -1;
			uint32_t len = rd32(p);
			if (len > left)
				return -1;
			p += len;
			left -= len;
		}
	}
	decode_video(s, p, left);
	s->offset += size;
	s->current++;
	return 0;
}

/* Audio rate flags. */
#define SMK_AUDIO_COMPRESSED 0x80000000u
#define SMK_AUDIO_PRESENT 0x40000000u
#define SMK_AUDIO_16BIT 0x20000000u
#define SMK_AUDIO_STEREO 0x10000000u

static size_t decode_audio_chunk(const uint8_t *p, size_t len, int bits16, int stereo,
	int16_t *out, size_t room)
{
	if (len < 4)
		return 0;
	uint32_t unpacked = rd32(p);
	bits b = { p + 4, len - 4, 0 };
	if (!get_bit(&b))
		return 0;
	stereo = get_bit(&b);
	bits16 = get_bit(&b);
	int ntrees = 1 << (stereo + bits16);
	tree t[4] = { { 0 } };
	for (int i = 0; i < ntrees; i++)
		read_tree8(&t[i], &b);
	size_t n = 0, samples = bits16 ? unpacked / 2 : unpacked;
	if (bits16) {
		int pred[2] = { 0, 0 };
		for (int i = stereo; i >= 0; i--) {
			unsigned v = get_bits(&b, 16);
			pred[i] = (int16_t)((v >> 8) | (v & 0xff) << 8);
		}
		size_t i = 0;
		for (; i <= (size_t)stereo && n < room; i++)
			out[n++] = (int16_t)pred[i];
		for (; i < samples && n < room; i++) {
			int ch = (int)(i & (size_t)stereo);
			unsigned lo = tree_decode(&t[ch * 2], &b), hi = tree_decode(&t[ch * 2 + 1], &b);
			pred[ch] = (int16_t)(pred[ch] + (int16_t)(lo | hi << 8));
			out[n++] = (int16_t)pred[ch];
		}
	}
	else {
		int pred[2] = { 0, 0 };
		for (int i = stereo; i >= 0; i--)
			pred[i] = (int)get_bits(&b, 8);
		size_t i = 0;
		for (; i <= (size_t)stereo && n < room; i++)
			out[n++] = (int16_t)((pred[i] - 128) * 256);
		for (; i < samples && n < room; i++) {
			int ch = (int)(i & (size_t)stereo);
			pred[ch] = (pred[ch] + (int8_t)tree_decode(&t[ch], &b)) & 0xff;
			out[n++] = (int16_t)((pred[ch] - 128) * 256);
		}
	}
	for (int i = 0; i < 4; i++)
		free(t[i].n);
	return n;
}

size_t smk_decode_audio(smacker *s, int16_t **out, int *rate, int *channels)
{
	uint32_t ar = s->audio_rate[0];
	*out = NULL;
	if (!(ar & SMK_AUDIO_PRESENT))
		return 0;
	int bits16 = !!(ar & SMK_AUDIO_16BIT), stereo = !!(ar & SMK_AUDIO_STEREO);
	*rate = (int)(ar & 0xffffff);
	*channels = stereo + 1;
	size_t cap = 0, n = 0, offset = s->first_frame;
	int16_t *buf = NULL;
	for (int f = 0; f < s->frames; f++) {
		uint32_t size = frame_size(s, f);
		uint8_t types = s->frame_types[f];
		const uint8_t *p = s->data + offset;
		size_t left = size;
		if (offset + size > s->size)
			break;
		if (types & 1) {
			size_t len = (size_t)p[0] * 4;
			p += len;
			left -= len < left ? len : left;
		}
		if ((types & 2) && left >= 4) {
			uint32_t len = rd32(p);
			if (len <= left) {
				size_t room = ar & SMK_AUDIO_COMPRESSED ? rd32(p + 4) : len - 4;
				size_t want = bits16 ? room / 2 + 2 : room + 2;
				if (n + want > cap) {
					cap = (n + want) * 2;
					buf = realloc(buf, cap * sizeof *buf);
				}
				if (ar & SMK_AUDIO_COMPRESSED) {
					n += decode_audio_chunk(p + 4, len - 4, bits16, stereo, buf + n, want);
				}
				else {
					const uint8_t *q = p + 4;
					for (size_t i = 0; i + (size_t)bits16 < len - 4; i += 1 + (size_t)bits16)
						buf[n++] = bits16 ? (int16_t)(q[i] | q[i + 1] << 8)
								  : (int16_t)((q[i] - 128) << 8);
				}
			}
		}
		offset += size;
	}
	*out = buf;
	return n / (size_t)*channels;
}
