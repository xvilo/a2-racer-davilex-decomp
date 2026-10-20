//! The camera (camera.cpp, and mycontro.cpp/opptools.cpp's camera modes).

use crate::race::mapexec::view_octant;
use crate::race::math3d::{cos_a, normalize_vector, sin_a, Matrix, ANGLE_MASK};
use crate::race::Race;

/// set_camera_position's modes: behind the car (F2).
pub const CAMERA_CHASE: i32 = 1;
/// From the driver's seat (F1).
pub const CAMERA_COCKPIT: i32 = 2;
/// Behind watched_opponent.
pub const CAMERA_OPPONENT: i32 = 3;
/// High above (a hidden key combination).
pub const CAMERA_OVERHEAD: i32 = 4;

pub struct Camera {
    pub camera_height: f32,
    pub camera_distance: f32,
    pub camera_position: i32,
    pub watched_opponent: usize,
    /// The player's car follows the road's slope: its pitch, yaw and roll for the matrix.
    pub car_pitch: i32,
    pub car_yaw: i32,
    pub car_roll: i32,
    /// DAT_0045b274: always 0.
    pub look_offset: i32,
    camera_x: f32,
    camera_y: f32,
    camera_z: f32,
    camera_yaw: f32,
    camera_pitch: f32,
    /// The road's slope under the car, smoothed.
    camera_tilt: f32,
    camera_roll: f32,
    camera_lag_xz: f32,
    camera_lag_y: f32,
    /// FLOAT_0045b264: 0.
    car_height_offset: f32,
    /// init_atan_table(4, 0, 1000, 1, -61, 256): the camera's downward pitch for a
    /// height above the car, looking at a point 61 ahead.
    atan_table: Vec<i32>,
}

impl Default for Camera {
    fn default() -> Camera {
        Camera {
            camera_height: 15.0,
            camera_distance: -31.0,
            camera_position: CAMERA_CHASE,
            watched_opponent: 0,
            car_pitch: 0,
            car_yaw: 0,
            car_roll: 0,
            look_offset: 0,
            camera_x: 0.0,
            camera_y: 0.0,
            camera_z: 0.0,
            camera_yaw: 0.0,
            camera_pitch: 0.0,
            camera_tilt: 0.0,
            camera_roll: 0.0,
            camera_lag_xz: 0.0,
            camera_lag_y: 0.0,
            car_height_offset: 0.0,
            atan_table: Vec::new(),
        }
    }
}

impl Camera {
    pub fn init_cameras(&mut self) {
        *self = Camera::default();
        self.atan_table = (0..1000)
            .map(|j| {
                let a = ((j as f32) as f64).atan2(-61.0);
                (-(((1.570798 - a) * 256.0) / 1.570798 + 256.0) + 0.5) as i32
            })
            .collect();
    }

    fn atan_lookup(&self, height: f32) -> i32 {
        let i = ((height + 0.5) as i32).clamp(0, 999);
        self.atan_table[i as usize]
    }
}

impl Race {
    /// Places the view: position (already negated), angles 0..1023.
    pub fn set_camera(&mut self, x: f32, y: f32, z: f32, pitch: i32, yaw: i32, roll: i32) {
        let mut r = Matrix::identity();
        r.rotate_x(pitch);
        r.rotate_z(roll);
        r.rotate_y(yaw);
        self.render.set_view(&r, &Matrix::translation(x, y, z));
    }

    fn player_visible(&mut self, visible: bool) {
        let i = self.player_index;
        self.pools.objects3d[i].visible = visible;
    }

    pub fn set_camera_position(&mut self, mode: i32) {
        match mode {
            CAMERA_CHASE => {
                self.player_visible(true);
                self.camera.camera_distance = -31.0;
                self.camera.camera_height = 15.0;
            }
            CAMERA_COCKPIT => {
                self.player_visible(false);
                self.camera.camera_distance = -4.0;
                self.camera.camera_height = 7.0;
            }
            CAMERA_OPPONENT => {
                self.camera.camera_distance = -31.0;
                self.camera.camera_height = 32.0;
            }
            CAMERA_OVERHEAD => {
                self.player_visible(true);
                self.camera.camera_height = 511.0;
                self.camera.camera_distance = -31.0;
            }
            _ => panic!("unknown camera position {mode}"),
        }
        self.camera.camera_position = mode;
    }

    /// The view's wedge and box, from the camera's place and direction.
    fn update_view_area(&mut self, fx: f32, fz: f32, side_heading: i32, back: f32) {
        let c = &self.camera;
        let (cx, cz) = (c.camera_x, c.camera_z);
        self.update_visible_tiles((-cx / 30.0) as i32, (-cz / 30.0) as i32, view_octant(fx, fz));
        let h = (side_heading - 0x100) & ANGLE_MASK;
        let x2 = (cos_a(h) * 30.0 * back + -cx) as i32;
        let z2 = (sin_a(h) * 30.0 * back + -cz) as i32;
        let sx = cos_a(side_heading) * 30.0 * 10.0;
        let sz = sin_a(side_heading) * 30.0 * 10.0;
        self.mapexec.set_view_quad((-cx + sx) as i32, (-cz + sz) as i32, x2, z2, (-cx - sx) as i32, (-cz - sz) as i32);
        self.get_road_height();
    }

    /// The road's slope under the car, front to back (tilt) and side to side (roll).
    fn follow_road(&mut self) {
        let h = self.player_heading + 0x100;
        let (fx, fz) = (cos_a(h) * 8.0, sin_a(h) * 8.0);
        self.player_chunk = self.update_chunk_for_position(self.player_chunk, self.player_x, self.player_z);
        let (px, pz, chunk) = (self.player_x, self.player_z, self.player_chunk);
        let c = &mut self.camera;
        c.camera_lag_xz = (3.0 - self.player_speed.abs()).max(1.0);
        self.player_y = ((self.weg.road_height_at(chunk, px, pz) + c.car_height_offset) - self.player_y) / c.camera_lag_xz
            + self.player_y;
        let back = self.weg.road_height_at(chunk, px - fx, pz - fz);
        let mut v = [2.0 * fx, self.weg.road_height_at(chunk, px + fx, pz + fz) - back, 2.0 * fz];
        normalize_vector(&mut v);
        c.camera_lag_y = (3.0 - self.player_speed).max(1.0);
        c.camera_tilt = (v[1] * 128.0 - c.camera_tilt) / c.camera_lag_y + c.camera_tilt;
        let s = self.player_heading;
        let (sx, sz) = (cos_a(s) * 4.0, sin_a(s) * 4.0);
        let back = self.weg.road_height_at(chunk, px - sx, pz - sz);
        let mut w = [2.0 * sx, self.weg.road_height_at(chunk, px + sx, pz + sz) - back, 2.0 * sz];
        normalize_vector(&mut w);
        c.camera_roll = (w[1] * 128.0 - c.camera_roll) / 3.0 + c.camera_roll;
    }

    /// update_camera_chase (and update_camera_overhead, the same from higher up).
    fn update_camera_chase(&mut self) {
        self.follow_road();
        let c = &mut self.camera;
        let (tilt, roll) = (c.camera_tilt as i32, c.camera_roll as i32);
        let h = self.player_heading - c.look_offset + 0x100;
        let (fx, fz) = (cos_a(h), sin_a(h));
        c.camera_x = (-(fx * c.camera_distance + self.player_x) - c.camera_x) * 0.25 + c.camera_x;
        c.camera_y = (-(self.player_y + c.camera_height) - c.camera_y) * 0.25 + c.camera_y;
        c.camera_z = (-(fz * c.camera_distance + self.player_z) - c.camera_z) * 0.25 + c.camera_z;
        let pitch_target = c.atan_lookup(c.camera_height);
        c.camera_yaw = ((self.player_heading as f32 - c.camera_yaw) - (c.look_offset / 2) as f32) / 3.0 + c.camera_yaw;
        c.camera_pitch = (pitch_target as f32 - c.camera_pitch) / 3.0 + c.camera_pitch;
        let (x, y, z, p, yw) = (c.camera_x, c.camera_y, c.camera_z, c.camera_pitch as i32, c.camera_yaw as i32);
        self.set_camera(x, y, z, p, yw, 0);
        self.update_view_area(fx, fz, self.player_heading, 10.0);
        let c = &mut self.camera;
        c.car_pitch = -tilt;
        c.car_yaw = -self.player_heading;
        c.car_roll = roll;
    }

    fn update_camera_cockpit(&mut self) {
        self.follow_road();
        let c = &mut self.camera;
        let (tilt, roll) = (c.camera_tilt as i32, c.camera_roll as i32);
        let h = self.player_heading + 0x100;
        let (fx, fz) = (cos_a(h), sin_a(h));
        // The original offsets z with the x component too (fx, not fz); kept.
        c.camera_x = -(self.player_x + fx * c.camera_distance);
        c.camera_y = -(self.player_y + c.camera_height);
        c.camera_z = -(fx * c.camera_distance + self.player_z);
        let (x, y, z) = (c.camera_x, c.camera_y, c.camera_z);
        self.set_camera(x, y, z, tilt, self.player_heading, -roll);
        self.update_view_area(fx, fz, self.player_heading, 10.0);
        let c = &mut self.camera;
        c.car_pitch = -tilt;
        c.car_yaw = -self.player_heading;
        c.car_roll = roll;
    }

    /// place_camera_behind: behind any car (the opponent camera).
    fn place_camera_behind(&mut self, x: f32, y: f32, z: f32, heading: i32) {
        let c = &mut self.camera;
        let h = heading + 0x100;
        let (fx, fz) = (cos_a(h), sin_a(h));
        c.camera_x = (-(fx * c.camera_distance + x) - c.camera_x) * 0.25 + c.camera_x;
        c.camera_y = (-(y + c.camera_height) - c.camera_y) * 0.25 + c.camera_y;
        c.camera_z = (-(fz * c.camera_distance + z) - c.camera_z) * 0.25 + c.camera_z;
        let pitch_target = c.atan_lookup(c.camera_height);
        c.camera_yaw = (heading as f32 - c.camera_yaw) / 3.0 + c.camera_yaw;
        c.camera_pitch = (pitch_target as f32 - c.camera_pitch) / 3.0 + c.camera_pitch;
        let (cx, cy, cz, p, yw) = (c.camera_x, c.camera_y, c.camera_z, c.camera_pitch as i32, c.camera_yaw as i32);
        self.set_camera(cx, cy, cz, p, yw, 0);
        self.update_view_area(fx, fz, heading, 5.0);
    }

    fn update_camera_opponent(&mut self) {
        let o = &self.opp.data[self.camera.watched_opponent];
        let heading = ((o.field_20.atan2(o.field_18) as f64) * 1024.0 / 6.283185307) as i32 - 0x100;
        let (x, y, z) = (o.x, o.y, o.z);
        self.place_camera_behind(x, y, z, heading);
    }

    /// Moves the camera after the player (update_camera).
    pub fn update_camera(&mut self) {
        match self.camera.camera_position {
            CAMERA_CHASE | CAMERA_OVERHEAD => self.update_camera_chase(),
            CAMERA_COCKPIT => {
                self.update_camera_cockpit();
                self.player_visible(false);
                return;
            }
            CAMERA_OPPONENT => self.update_camera_opponent(),
            m => panic!("unknown camera position {m}"),
        }
        self.player_visible(true);
    }

    /// The swoop down at the start (start_camera_intro).
    pub fn start_camera_intro(&mut self) {
        self.set_camera_position(CAMERA_CHASE);
        self.player_visible(true);
        let c = &mut self.camera;
        c.camera_height = 350.0;
        c.camera_distance = -31.0;
        c.camera_pitch = c.atan_lookup(350.0) as f32;
        c.camera_yaw = self.player_heading as f32;
        c.camera_x = -self.player_x;
        c.camera_y = -self.player_y - c.camera_height;
        c.camera_z = -self.player_z;
    }
}
