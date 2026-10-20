//! What is in view (mapexec.cpp, weg.cpp's compute_road_height): a box of the map ahead
//! of the camera and a wedge opening forward from just behind it. Objects inside the box
//! are drawn; road chunks inside both.

use crate::race::Race;

/// Tiles of 12 * 30 units, 6 of them across the box (tile_size, DAT_004596d8..e4).
const TILE_SIZE: i32 = 12;
const TILE_SCALE: i32 = 12;
const TILES_X: i32 = 6;
const TILES_Z: i32 = 6;
const MAP_MAX: i32 = 200000;

#[derive(Default)]
pub struct MapExec {
    /// The wedge's points, as the original stores them.
    quad: [i32; 8],
    view_min_x: i32,
    view_max_x: i32,
    view_min_z: i32,
    view_max_z: i32,
}

/// point_side_of_line: which side of the line from (l[0], l[1]) to (l[2], l[3]).
fn point_side_of_line(l: &[i32], x: i32, z: i32) -> i32 {
    let (dx, dz) = (l[2].wrapping_sub(l[0]), l[3].wrapping_sub(l[1]));
    x.wrapping_sub(l[0]).wrapping_mul(dz).wrapping_sub(dx.wrapping_mul(z.wrapping_sub(l[1]))).signum()
}

/// The octant of a direction (x, z) as the camera code picks it.
pub fn view_octant(x: f32, z: f32) -> i32 {
    if x < -0.5 {
        if z < -0.5 { 5 } else if z > 0.5 { 7 } else { 6 }
    } else if x > 0.5 {
        if z < -0.5 { 3 } else if z > 0.5 { 1 } else { 2 }
    } else if z < -0.5 {
        4
    } else if z > 0.5 {
        0
    } else {
        1
    }
}

impl MapExec {
    /// The wedge: lines p3 -> p2 and p2 -> p1 (p2 behind the camera).
    pub fn set_view_quad(&mut self, x1: i32, z1: i32, x2: i32, z2: i32, x3: i32, z3: i32) {
        // DAT_00477024..40: line (x2,z2)-(x1,z1), and line (x3,z3)-(x2,z2).
        self.quad = [x2, z2, x1, z1, x3, z3, x2, z2];
    }

    fn point_in_view_quad(&self, x: i32, z: i32) -> bool {
        point_side_of_line(&self.quad[4..], x, z) < 0 && point_side_of_line(&self.quad[0..], x, z) < 0
    }
}

impl Race {
    /// The box around map position (x, z) in units of 30, placed ahead for the octant
    /// the camera looks into (0..7); also hides objects outside it.
    pub fn update_visible_tiles(&mut self, x: i32, z: i32, octant: i32) {
        let (w, h) = (TILE_SIZE * TILES_X, TILE_SCALE * TILES_Z);
        let (mut lx, mut rx, mut lz, mut hz) = match octant {
            0 => (x - w / 2, x + w / 2, z - 2 * TILE_SCALE, z + h - 2 * TILE_SCALE),
            1 => (x - 2 * TILE_SIZE, x + w - 2 * TILE_SIZE, z - 2 * TILE_SCALE, z + h - 2 * TILE_SCALE),
            2 => (x - 2 * TILE_SIZE, x + w - 2 * TILE_SIZE, z - h / 2, z + h / 2),
            3 => (x - 2 * TILE_SIZE, x + w - 2 * TILE_SIZE, z - h + 2 * TILE_SCALE, z + 2 * TILE_SCALE),
            4 => (x - w / 2, x + w / 2, z - h + 2 * TILE_SCALE, z + 2 * TILE_SCALE),
            5 => (x - w + 2 * TILE_SIZE, x + 2 * TILE_SIZE, z - h + 2 * TILE_SCALE, z + 2 * TILE_SCALE),
            6 => (x - w + 2 * TILE_SIZE, x + 2 * TILE_SIZE, z - h / 2, z + h / 2),
            7 => (x - w + 2 * TILE_SIZE, x + 2 * TILE_SIZE, z - 2 * TILE_SCALE, z + h - 2 * TILE_SCALE),
            _ => (0, 0, 0, 0),
        };
        if lx < 0 {
            lx = 0;
            rx = w;
        }
        if lz < 0 {
            lz = 0;
            hz = h;
        }
        if MAP_MAX < hz {
            lz = MAP_MAX - h;
            hz = MAP_MAX;
        }
        if MAP_MAX < rx {
            lx = MAP_MAX - w;
            rx = MAP_MAX;
        }
        let m = &mut self.mapexec;
        m.view_min_x = lx * 30;
        m.view_max_x = rx * 30;
        m.view_min_z = lz * 30;
        m.view_max_z = hz * 30;
        let (a, b, c, d) = (m.view_min_x, m.view_min_z, m.view_max_x, m.view_max_z);
        self.pools.check_objects3d_visibility(a, b, c, d);
    }

    fn in_box(&self, vertex: usize) -> bool {
        let v = &self.pools.vertices3d[vertex];
        let m = &self.mapexec;
        (m.view_min_x as f32) < v.x && v.x < m.view_max_x as f32 && (m.view_min_z as f32) < v.z && v.z < m.view_max_z as f32
    }

    fn in_wedge(&self, vertex: usize) -> bool {
        let v = &self.pools.vertices3d[vertex];
        self.mapexec.point_in_view_quad(v.x as i32, v.z as i32)
    }

    /// Marks the road chunks in view (get_road_height / compute_road_height).
    pub fn get_road_height(&mut self) {
        let (mut first, mut last) = (-1, -1);
        // The original tests points up to the third-last: every chunk but the last.
        for i in 0..(self.weg.chunk_count - 1).max(0) {
            let q = self.weg.chunk_quads[i as usize];
            let s = &self.weg.sections[self.weg.chunks[q].section];
            let far = if s.ground_count > 10 { 10 } else { s.ground_count.saturating_sub(1) };
            let (point, er, el, gr, gl) = (s.point, s.edge_right, s.edge_left, s.ground_right[far], s.ground_left[far]);
            self.weg.chunks[q].visible = false;
            if !self.in_wedge(point) && !self.in_wedge(gr) && !self.in_wedge(gl) {
                continue;
            }
            if self.in_box(point) || self.in_box(er) || self.in_box(el) || self.in_box(gr) || self.in_box(gl) {
                self.weg.chunks[q].visible = true;
                if first < 0 {
                    first = i;
                }
                last = i;
            }
        }
        self.weg.set_visible_weg_chunks(first, last + 1);
    }
}
