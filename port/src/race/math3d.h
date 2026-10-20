/* Angles, tables and matrices (math3d.cpp). Angles are 0..1023 for a full turn.
 * Matrices are Direct3D's D3DMATRIX: row-major, applied to row vectors (v' = v * M),
 * which is the same memory layout OpenGL's glLoadMatrixf expects. */
#ifndef A2_MATH3D_H
#define A2_MATH3D_H

#define ANGLE_STEPS 1024
#define ANGLE_MASK 0x3ff

typedef struct {
	float m[16];
} matrix_t;

/* The original's tables; its globals are named the other way round (sin_table holds
 * the cosines), the port names them by content. */
extern float cos_table[ANGLE_STEPS];
extern float sin_table[ANGLE_STEPS];

void init_math3d(void);

void matrix_identity(matrix_t *m);
/* a = b * a: applies b first (matrix_multiply in the original). */
void matrix_multiply(matrix_t *a, const matrix_t *b);
/* m = rotation * m, for an angle 0..1023 (rotate_matrix_x/y/z). */
void rotate_matrix_x(matrix_t *m, int angle);
void rotate_matrix_y(matrix_t *m, int angle);
void rotate_matrix_z(matrix_t *m, int angle);
void matrix_translation(matrix_t *m, float x, float y, float z);

void normalize_vector(float v[3]);
void cross_product(float out[3], const float a[3], const float b[3]);
void vector_subtract(float out[3], const float a[3], const float b[3]);

#endif
