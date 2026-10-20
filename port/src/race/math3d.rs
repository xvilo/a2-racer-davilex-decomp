//! Angles, tables and matrices (math3d.cpp). Angles are 0..1023 for a full turn.
//! Matrices are Direct3D's D3DMATRIX: row-major, applied to row vectors (v' = v * M),
//! which is the same memory layout OpenGL's glLoadMatrixf expects.

use std::sync::LazyLock;

pub const ANGLE_STEPS: usize = 1024;
pub const ANGLE_MASK: i32 = 0x3ff;

/// The original's slightly-off pi.
fn table(f: fn(f64) -> f64) -> [f32; ANGLE_STEPS] {
    std::array::from_fn(|i| f(i as f64 * (1.0 / 1024.0) * 2.0 * 3.141596) as f32)
}

/// The original's tables; its globals are named the other way round (sin_table holds
/// the cosines), the port names them by content. init_math3d filled them.
pub static COS_TABLE: LazyLock<[f32; ANGLE_STEPS]> = LazyLock::new(|| table(f64::cos));
pub static SIN_TABLE: LazyLock<[f32; ANGLE_STEPS]> = LazyLock::new(|| table(f64::sin));

/// The table entries for an angle (any int; only its low ten bits count).
pub fn cos_a(angle: i32) -> f32 {
    COS_TABLE[(angle & ANGLE_MASK) as usize]
}

pub fn sin_a(angle: i32) -> f32 {
    SIN_TABLE[(angle & ANGLE_MASK) as usize]
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Matrix {
    pub m: [f32; 16],
}

impl Matrix {
    pub fn identity() -> Matrix {
        let mut m = [0.0; 16];
        m[0] = 1.0;
        m[5] = 1.0;
        m[10] = 1.0;
        m[15] = 1.0;
        Matrix { m }
    }

    /// self = b * self: applies b first (matrix_multiply in the original).
    pub fn multiply(&mut self, b: &Matrix) {
        let mut r = [0.0f32; 16];
        for row in 0..4 {
            for col in 0..4 {
                r[row * 4 + col] = (0..4).map(|k| b.m[row * 4 + k] * self.m[k * 4 + col]).sum();
            }
        }
        self.m = r;
    }

    /// self = rotation * self, for an angle 0..1023 (rotate_matrix_x).
    pub fn rotate_x(&mut self, angle: i32) {
        let (c, s) = (cos_a(angle), sin_a(angle));
        let mut r = Matrix::identity();
        r.m[5] = c;
        r.m[6] = s;
        r.m[9] = -s;
        r.m[10] = c;
        self.multiply(&r);
    }

    pub fn rotate_y(&mut self, angle: i32) {
        let (c, s) = (cos_a(angle), sin_a(angle));
        let mut r = Matrix::identity();
        r.m[0] = c;
        r.m[2] = -s;
        r.m[8] = s;
        r.m[10] = c;
        self.multiply(&r);
    }

    pub fn rotate_z(&mut self, angle: i32) {
        let (c, s) = (cos_a(angle), sin_a(angle));
        let mut r = Matrix::identity();
        r.m[0] = c;
        r.m[1] = s;
        r.m[4] = -s;
        r.m[5] = c;
        self.multiply(&r);
    }

    pub fn translation(x: f32, y: f32, z: f32) -> Matrix {
        let mut m = Matrix::identity();
        m.m[12] = x;
        m.m[13] = y;
        m.m[14] = z;
        m
    }
}

pub fn normalize_vector(v: &mut [f32; 3]) {
    let l = (v[0] * v[0] + v[1] * v[1] + v[2] * v[2]).sqrt();
    if l > 0.0 {
        v.iter_mut().for_each(|x| *x /= l);
    }
}

#[allow(dead_code)]
pub fn cross_product(a: &[f32; 3], b: &[f32; 3]) -> [f32; 3] {
    [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]
}

pub fn vector_subtract(a: &[f32; 3], b: &[f32; 3]) -> [f32; 3] {
    [a[0] - b[0], a[1] - b[1], a[2] - b[2]]
}
