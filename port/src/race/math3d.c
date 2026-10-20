#include "race/math3d.h"

#include <math.h>
#include <string.h>

float cos_table[ANGLE_STEPS];
float sin_table[ANGLE_STEPS];

void init_math3d(void)
{
	for (int i = 0; i < ANGLE_STEPS; i++) {
		/* The original's slightly-off pi. */
		double a = i * (1.0 / 1024.0) * 2.0 * 3.141596;
		cos_table[i] = (float)cos(a);
		sin_table[i] = (float)sin(a);
	}
}

void matrix_identity(matrix_t *m)
{
	memset(m, 0, sizeof *m);
	m->m[0] = m->m[5] = m->m[10] = m->m[15] = 1.0f;
}

void matrix_multiply(matrix_t *a, const matrix_t *b)
{
	matrix_t r;
	for (int row = 0; row < 4; row++) {
		for (int col = 0; col < 4; col++) {
			float s = 0;
			for (int k = 0; k < 4; k++)
				s += b->m[row * 4 + k] * a->m[k * 4 + col];
			r.m[row * 4 + col] = s;
		}
	}
	*a = r;
}

void rotate_matrix_x(matrix_t *m, int angle)
{
	matrix_t r;
	float c = cos_table[angle & ANGLE_MASK], s = sin_table[angle & ANGLE_MASK];
	matrix_identity(&r);
	r.m[5] = c;
	r.m[6] = s;
	r.m[9] = -s;
	r.m[10] = c;
	matrix_multiply(m, &r);
}

void rotate_matrix_y(matrix_t *m, int angle)
{
	matrix_t r;
	float c = cos_table[angle & ANGLE_MASK], s = sin_table[angle & ANGLE_MASK];
	matrix_identity(&r);
	r.m[0] = c;
	r.m[2] = -s;
	r.m[8] = s;
	r.m[10] = c;
	matrix_multiply(m, &r);
}

void rotate_matrix_z(matrix_t *m, int angle)
{
	matrix_t r;
	float c = cos_table[angle & ANGLE_MASK], s = sin_table[angle & ANGLE_MASK];
	matrix_identity(&r);
	r.m[0] = c;
	r.m[1] = s;
	r.m[4] = -s;
	r.m[5] = c;
	matrix_multiply(m, &r);
}

void matrix_translation(matrix_t *m, float x, float y, float z)
{
	matrix_identity(m);
	m->m[12] = x;
	m->m[13] = y;
	m->m[14] = z;
}

void normalize_vector(float v[3])
{
	float l = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
	if (l > 0) {
		v[0] /= l;
		v[1] /= l;
		v[2] /= l;
	}
}

void cross_product(float out[3], const float a[3], const float b[3])
{
	float r[3] = {
		a[1] * b[2] - a[2] * b[1],
		a[2] * b[0] - a[0] * b[2],
		a[0] * b[1] - a[1] * b[0],
	};
	memcpy(out, r, sizeof r);
}

void vector_subtract(float out[3], const float a[3], const float b[3])
{
	out[0] = a[0] - b[0];
	out[1] = a[1] - b[1];
	out[2] = a[2] - b[2];
}
