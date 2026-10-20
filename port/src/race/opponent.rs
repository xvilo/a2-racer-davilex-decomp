//! Everything that moves or collides on the road (opponent.cpp, active.cpp, objio.cpp):
//! the player, the rivals, the police and the traffic, and the scenery objects'
//! collision boxes. One Opponent per object; the fields follow the original's 0x100-byte
//! entries.

use crate::race::driver::DRIVER_RIVAL;
use crate::race::math3d::{cos_a, normalize_vector, sin_a, vector_subtract, Matrix};
use crate::race::object3d::Update;
use crate::race::player::CAR_TYPES;
use crate::race::Race;

/// lane_offsets: the lanes' distances from the centre (positive: to the right).
pub const LANE_OFFSETS: [f32; 11] = [108.0, 85.0, 60.0, 30.0, 0.0, 0.0, 0.0, -30.0, -60.0, -85.0, -108.0];

#[derive(Clone, Default)]
pub struct Opponent {
    /// Index into the car type table.
    pub car_type: i32,
    pub x: f32,
    pub y: f32,
    pub z: f32,
    pub speed: f32,
    pub acceleration: f32,
    /// The direction of travel (x, -, z).
    pub field_18: f32,
    pub field_1c: i32,
    pub field_20: f32,
    /// 0..1023
    pub heading: i32,
    pub steer_angle: i32,
    /// Added to steer_angle each frame.
    pub steer_rate: i32,
    /// Weights the collision response.
    pub mass: f32,
    pub distance_to_target: f32,
    pub max_speed: f32,
    /// Copied from the car type table.
    pub car_class: i32,
    /// Collision radius.
    pub radius: f32,
    /// 4 bounding-box corners (x, y, z), rotated by heading.
    pub world_corners: [f32; 12],
    /// The same corners relative to the object.
    pub local_corners: [f32; 12],
    pub target_x: f32,
    pub target_y: f32,
    pub target_z: f32,
    /// Road chunk the car is on.
    pub chunk: i32,
    pub lane: i32,
    pub target_lane: i32,
    pub prev_waypoint: i32,
    pub waypoint: i32,
    /// 0 or 1, driving direction along the road.
    pub direction: i32,
    /// Index into objects3d.
    pub object: i32,
    /// The driver (index into driver_list), -1 for none.
    pub field_e0: i32,
    /// The kind of driver (DRIVER_*).
    pub field_e4: i32,
    pub active: bool,
    /// Collision push.
    pub push_x: f32,
    pub push_z: f32,
    pub offset_x: f32,
    pub offset_z: f32,
    pub heading_delta: i32,
}

pub struct Opponents {
    pub data: Vec<Opponent>,
    /// Slots handed out so far.
    pub opponents_used: usize,
    /// The cars near the player that move this frame, and those near enough to collide.
    active_list: Vec<usize>,
    nearby_list: Vec<usize>,
    pub police_max_speed: i32,
    pub racer_speed: i32,
}

impl Default for Opponents {
    fn default() -> Opponents {
        Opponents {
            data: Vec::new(),
            opponents_used: 0,
            active_list: Vec::new(),
            nearby_list: Vec::new(),
            police_max_speed: 17,
            racer_speed: 16,
        }
    }
}

impl Opponents {
    pub fn count(&self) -> usize {
        self.data.len()
    }
}

/// Whether any of the five points (corners and centre) is inside the quad c[0..3].
fn points_in_quad(pts: &[[f32; 3]; 5], c: &[[f32; 3]; 5]) -> bool {
    let cross_y = |a: &[f32; 3], b: &[f32; 3]| a[0] * b[2] - a[2] * b[0];
    let e0 = vector_subtract(&c[1], &c[0]);
    let e1 = vector_subtract(&c[2], &c[1]);
    let e2 = vector_subtract(&c[3], &c[2]);
    let e3 = vector_subtract(&c[0], &c[3]);
    pts.iter().any(|p| {
        cross_y(&e0, &vector_subtract(p, &c[0])) >= 0.0
            && cross_y(&e2, &vector_subtract(p, &c[2])) >= 0.0
            && cross_y(&e1, &vector_subtract(p, &c[1])) >= 0.0
            && cross_y(&e3, &vector_subtract(p, &c[3])) >= 0.0
    })
}

impl Race {
    pub fn init_opponents(&mut self, count: usize) {
        if !self.opp.data.is_empty() {
            return;
        }
        self.opp.data = vec![Opponent { field_e0: -1, ..Default::default() }; count];
        self.opp.opponents_used = 0;
    }

    pub fn alloc_opponent_slot(&mut self) -> usize {
        if self.opp.opponents_used < self.opp.count() {
            self.opp.opponents_used += 1;
            return self.opp.opponents_used - 1;
        }
        self.opp.data.iter().position(|o| !o.active).unwrap_or_else(|| panic!("no more free opponent slots"))
    }

    /// objio.cpp: the object's bounding rectangle and radius into its opponent entry.
    pub fn compute_object_bounds(&mut self, object: usize) {
        let op = self.pools.objects3d[object].opponent;
        if op < 0 || op as usize > self.opp.count() {
            panic!("illegal opponent {op}");
        }
        let (mut min_x, mut min_z, mut max_x, mut max_z) = (10000.0f32, 10000.0f32, 0.0f32, 0.0f32);
        // The original tests the in-use flag of pool vertex i rather than of the frame's
        // i-th vertex; those are always in use, so every vertex counts.
        for &v in &self.pools.object_frame(object).vertices {
            let v = &self.pools.vertices3d[v as usize];
            min_x = if v.x < min_x { v.x } else { min_x };
            min_z = if v.z < min_z { v.z } else { min_z };
            max_x = if max_x < v.x { v.x } else { max_x };
            max_z = if max_z < v.z { v.z } else { max_z };
        }
        let o = &mut self.opp.data[op as usize];
        o.local_corners = [min_x, 0.0, min_z, max_x, 0.0, min_z, max_x, 0.0, max_z, min_x, 0.0, max_z];
        o.radius = (max_z * max_z + max_x * max_x).sqrt();
    }

    /// The four corners of the object's box, turned to its heading (update_racer_corners).
    pub fn update_racer_corners(&mut self, opponent: usize) {
        let o = &mut self.opp.data[opponent];
        let (c, s) = (cos_a(o.heading), sin_a(o.heading));
        for i in 0..4 {
            let (lx, lz) = (o.local_corners[i * 3], o.local_corners[i * 3 + 2]);
            o.world_corners[i * 3] = (lx * c - lz * s) + o.x;
            o.world_corners[i * 3 + 2] = lz * c + lx * s + o.z;
        }
    }

    /// Keeps a car between the median and the road's edge; true if it hit one.
    pub fn collide_racer_with_road_edge(&mut self, opponent: usize) -> bool {
        let o = &self.opp.data[opponent];
        let sec = &self.weg.sections[o.chunk as usize];
        let p = self.pools.vertices3d[sec.point];
        let (c, s) = (cos_a(sec.heading), sin_a(sec.heading));
        // How far this frame's move goes across the road.
        let mut moved = c * o.offset_z + -s * o.offset_x;
        let mut allowed = moved;
        if moved == 0.0 {
            return false;
        }
        let dist: Vec<f32> = (0..4)
            .map(|i| (-s * (o.world_corners[i * 3] - p.x) + c * (o.world_corners[i * 3 + 2] - p.z)).abs())
            .collect();
        if o.direction == 1 {
            allowed = -allowed;
            moved = -moved;
        }
        let mut hit = false;
        for d in dist {
            if 120.0 < d + allowed {
                hit = true;
                allowed = 120.0 - d;
            }
            if d + allowed < 5.0 {
                hit = true;
                allowed = 5.0 - d;
            }
        }
        if !hit {
            return false;
        }
        let o = &mut self.opp.data[opponent];
        o.offset_x *= allowed / moved;
        o.offset_z = (allowed / moved) * o.offset_z;
        o.speed *= 0.845;
        let (x, y, z, speed) = (o.x, o.y, o.z, o.speed);
        if 4.0 < speed {
            self.play_bump3_sound(x, y, z);
        }
        true
    }

    /// Stops the player at the track's ends.
    pub fn collide_player_with_track_ends(&mut self) {
        let o = &self.opp.data[0];
        let chunk = o.chunk;
        if chunk >= 6 && chunk < self.weg.chunk_count - 6 {
            return;
        }
        let sec = &self.weg.sections[chunk as usize];
        let p = self.pools.vertices3d[sec.point];
        let (c, s) = (cos_a(sec.heading), sin_a(sec.heading));
        let moved = s * o.offset_z + c * o.offset_x;
        let mut allowed = moved;
        if moved == 0.0 {
            return;
        }
        let mut hit = false;
        for i in 0..4 {
            let d = (c * (o.world_corners[i * 3] - p.x) + s * (o.world_corners[i * 3 + 2] - p.z)).abs();
            if o.direction == 1 {
                if d + allowed < 1.0 {
                    hit = true;
                    allowed = 1.0 - d;
                }
            } else if 59.0 < d + allowed {
                hit = true;
                allowed = 59.0 - d;
            }
        }
        if !hit {
            return;
        }
        let o = &mut self.opp.data[0];
        o.offset_x *= allowed / moved;
        o.offset_z = (allowed / moved) * o.offset_z;
        o.speed *= 0.845;
        let (x, y, z, speed) = (o.x, o.y, o.z, o.speed);
        if 4.0 < speed {
            self.play_bump3_sound(x, y, z);
        }
    }

    /// The lane (0..10) a car is in, or -1 off the road.
    pub fn get_racer_lane(&self, opponent: usize) -> i32 {
        const BOUNDS: [f32; 12] = [-120.0, -94.5, -72.5, -45.0, -15.0, 0.0, 0.0, 15.0, 45.0, 72.5, 94.5, 120.0];
        let o = &self.opp.data[opponent];
        let sec = &self.weg.sections[o.chunk as usize];
        let p = self.pools.vertices3d[sec.point];
        let h = sec.heading;
        let d = (o.z - p.z) * cos_a(h) + -sin_a(h) * (o.x - p.x);
        (0..11).find(|&i| BOUNDS[i] <= d && d < BOUNDS[i + 1]).map(|i| i as i32).unwrap_or(-1)
    }

    // --- creating and placing (opponent.cpp) ----------------------------------------

    /// The road's slope under a car: its pitch and roll (FUN_00405058), and the road's
    /// height there.
    fn opponent_slope(&self, opponent: usize, heading: i32) -> (f32, i32, i32) {
        let o = &self.opp.data[opponent];
        let chunk = self.update_chunk_for_position(o.chunk, o.x, o.z);
        let y = self.road_height_at(chunk, o.x, o.z);
        let (fx, fz) = (o.x + cos_a(heading) * 8.0, sin_a(heading) * 8.0 + o.z);
        let mut v = [fx - o.x, self.road_height_at(chunk, fx, fz) - y, fz - o.z];
        normalize_vector(&mut v);
        let pitch = (v[1] * 128.0) as i32;
        let s = heading + 0x100;
        let (sx, sz) = (cos_a(s) * 4.0, sin_a(s) * 4.0);
        let a = self.road_height_at(chunk, o.x + sx, o.z + sz);
        let b = self.road_height_at(chunk, o.x - sx, o.z - sz);
        let mut w = [2.0 * sx, a - b, 2.0 * sz];
        normalize_vector(&mut w);
        (y, pitch, (w[1] * 128.0) as i32)
    }

    pub fn update_opponent_object(&mut self, object: usize) {
        let op = self.pools.objects3d[object].opponent as usize;
        let o = &self.opp.data[op];
        let heading = 0x3ff - ((o.heading + o.steer_angle) & 0x3ff);
        let (mut pitch, mut roll) = (0, 0);
        if self.pools.objects3d[object].visible {
            let (y, p, r) = self.opponent_slope(op, heading);
            self.opp.data[op].y = y;
            pitch = p;
            roll = r;
        }
        let o = &self.opp.data[op];
        let ob = &mut self.pools.objects3d[object];
        ob.x = o.x;
        ob.y = o.y;
        ob.z = o.z;
        if ob.visible {
            let mut m = Matrix::identity();
            m.m[12] = ob.x;
            m.m[13] = ob.y;
            m.m[14] = ob.z;
            m.rotate_y(heading);
            m.rotate_x(-pitch);
            m.rotate_z(roll);
            ob.matrix = m;
        }
    }

    /// create_opponent: a car of a type, for a kind of driver; its object.
    pub fn create_opponent(&mut self, car_type: usize, kind: i32) -> usize {
        let object = self.pools.get_new_object3d();
        self.pools.objects3d[object].in_use = true;
        let op = self.alloc_opponent_slot();
        let anim = self.pools.init_anim3d(1);
        self.pools.objects3d[object].anim = anim;
        self.pools.objects3d[object].frame = 0;
        let ct = &CAR_TYPES[car_type];
        if self.player.car_mesh_id[car_type] == -1 {
            let data = self.files.race_file(ct.mesh).unwrap_or_else(|| panic!("Unable to load object {}", ct.mesh)).to_vec();
            self.pools.load_object_mesh(object, &data, ct.mesh);
            self.player.car_mesh_id[car_type] = self.pools.object_frame_index(object);
        } else {
            self.pools.anims3d[anim as usize].frames[0] = self.player.car_mesh_id[car_type];
        }
        let o = &mut self.opp.data[op];
        o.car_type = car_type as i32;
        o.object = object as i32;
        o.car_class = ct.car_class;
        o.max_speed = if car_type == 0xc { self.opp.police_max_speed as f32 } else { ct.max_speed };
        o.active = true;
        o.heading = 0;
        o.steer_angle = 0;
        o.steer_rate = 0;
        o.push_x = 0.0;
        o.push_z = 0.0;
        o.offset_x = 0.0;
        o.offset_z = 0.0;
        o.field_18 = cos_a(o.heading);
        o.field_1c = 0;
        o.field_20 = sin_a(o.heading);
        // The original tests the kind before setting it, so this is 0.2 for every car.
        let rival = o.field_e4 == DRIVER_RIVAL;
        let r = if rival { self.rand() } else { 0 };
        let o = &mut self.opp.data[op];
        o.acceleration = if rival { 0.2 - (r % 5) as f32 / 10.0 } else { 0.2 };
        o.mass = ct.mass;
        let texture = self.player.car_texture_id[car_type];
        let ob = &mut self.pools.objects3d[object];
        ob.texture = texture;
        ob.matrix = Matrix::identity();
        ob.update = Update::Opponent;
        ob.field_36 = 1;
        ob.visible = true;
        ob.opponent = op as i32;
        ob.update_interval = 1;
        self.build_object_execute_buffer(object);
        self.compute_object_bounds(object);
        let driver = self.get_new_driver();
        let o = &mut self.opp.data[op];
        o.field_e0 = driver as i32;
        o.field_e4 = kind;
        object
    }

    /// The point `lane` across and `along` behind a chunk's centre.
    fn lane_position(&mut self, op: usize, chunk: i32, lane: i32, along: f32) {
        let ch = &self.weg.chunks[chunk as usize];
        let p = self.pools.vertices3d[self.weg.sections[ch.section].point];
        let l = LANE_OFFSETS[lane as usize] as i32 as f32;
        let o = &mut self.opp.data[op];
        o.x = ch.side[0] * l + p.x;
        o.y = ch.side[1] * l + p.y;
        o.z = ch.side[2] * l + p.z;
        o.x -= ch.side[2] * along;
        o.z += along * ch.side[0];
    }

    /// next_opponent_waypoint: the chunk `ahead` chunks on; past the track's end the car
    /// starts again from the other end.
    fn next_opponent_waypoint(&mut self, op: usize, ahead: i32) {
        let count = self.weg.chunk_count;
        let o = &mut self.opp.data[op];
        if o.waypoint < 0 {
            if o.prev_waypoint < 0 {
                panic!("illegal waypoints for opponent {op}");
            }
            o.waypoint = o.prev_waypoint;
            return;
        }
        o.prev_waypoint = o.waypoint;
        let restart;
        if o.direction == 1 {
            o.waypoint = o.chunk + ahead;
            if o.waypoint <= count - 1 {
                return;
            }
            o.chunk = 3;
            o.waypoint = 5;
            restart = o.chunk;
            o.heading = self.weg.sections[o.chunk as usize].heading - 0x100;
        } else {
            o.waypoint = o.chunk - ahead;
            if o.waypoint >= 0 {
                return;
            }
            o.chunk = count - 4;
            o.waypoint = count - 6;
            restart = o.chunk;
            o.heading = self.weg.sections[o.chunk as usize].heading + 0x100;
        }
        let lane = o.lane;
        self.lane_position(op, restart, lane, -40.0);
        let o = &mut self.opp.data[op];
        o.offset_x = 0.0;
        o.offset_z = 0.0;
        o.push_x = 0.0;
        o.push_z = 0.0;
        o.heading_delta = 0;
    }

    /// compute_opponent_target: steer toward the target lane at the waypoint.
    fn compute_opponent_target(&mut self, op: usize) -> [f32; 3] {
        let o = &self.opp.data[op];
        let ch = &self.weg.chunks[o.waypoint as usize];
        let p = self.pools.vertices3d[self.weg.sections[ch.section].point];
        let l = LANE_OFFSETS[o.target_lane as usize] as i32 as f32;
        let side = ch.side;
        let o = &mut self.opp.data[op];
        o.target_x = side[0] * l + p.x;
        o.target_y = side[1] * l + p.y;
        o.target_z = l * side[2] + p.z;
        let t = [o.target_x, o.target_y, o.target_z];
        let mut dir = vector_subtract(&t, &[o.x, o.y, o.z]);
        normalize_vector(&mut dir);
        // cross_product_y with the car's direction (field_18, field_1c, field_20).
        let c = dir[0] * o.field_20 - dir[2] * o.field_18;
        let turn = (c.abs() * 32.0 * 1.5) as i32;
        o.heading_delta = if c < 0.0 { turn } else { -turn };
        t
    }

    fn move_opponent_toward_target(&mut self, op: usize) {
        let t = self.compute_opponent_target(op);
        let o = &mut self.opp.data[op];
        if o.speed < o.max_speed {
            o.speed += o.acceleration;
            if o.max_speed < o.speed {
                o.speed = o.max_speed;
            }
        } else {
            o.speed = o.max_speed;
        }
        o.distance_to_target = (o.z - t[2]).abs() + (o.x - t[0]).abs() + (o.y - t[1]).abs();
    }

    fn evaluate_opponent(&mut self, op: usize) {
        let o = &self.opp.data[op];
        if !o.active {
            return;
        }
        // The car's engine, heard nearer than 500.
        let distance = (o.z - self.player_z).abs() + (o.y - self.player_y).abs() + (o.x - self.player_x).abs();
        let (object, x, y, z) = (o.object, o.x, o.y, o.z);
        if 500.0 <= distance {
            self.stop_object_sound(object);
        } else {
            self.update_object_sound(object, x, y, z, distance);
        }
        self.move_opponent_toward_target(op);
        let o = &mut self.opp.data[op];
        o.steer_angle += o.steer_rate;
        o.steer_angle = (o.steer_angle as f32 * 0.95) as i32;
        o.steer_rate = (o.steer_rate as f32 * 0.95) as i32;
        o.push_x *= 0.75;
        o.push_z *= 0.75;
    }

    /// place_opponent: a racer or police car on a chunk and lane.
    pub fn place_opponent(&mut self, op: usize, direction: i32, chunk: i32, lane: i32, along: f32) {
        let o = &mut self.opp.data[op];
        o.direction = direction;
        o.target_lane = lane;
        o.lane = lane;
        self.lane_position(op, chunk, lane, along + 40.0);
        self.opp.data[op].chunk = chunk;
        self.next_opponent_waypoint(op, 2);
        self.set_map_occupant(op as i32, chunk, lane);
        let count = self.weg.chunk_count;
        let heading = self.weg.sections[chunk as usize].heading + if direction == 0 { 0x100 } else { -0x100 };
        let o = &mut self.opp.data[op];
        o.prev_waypoint = if direction == 0 { (chunk + 1) % (count - 1) } else { chunk - 1 };
        let (driver, kind) = (o.field_e0, o.field_e4);
        if driver != -1 {
            self.assign_driver_to_opponent(driver as usize, op, kind);
        }
        let o = &mut self.opp.data[op];
        o.heading = heading;
        o.speed = 0.0;
        o.field_18 = 0.0;
        o.field_1c = 0;
        o.field_20 = 0.0;
        o.heading_delta = 0;
        o.max_speed = 0.0;
    }

    /// scatter_traffic: every car from `first` on at a random free place, mostly driving
    /// the stage's way, not within `margin` chunks of the ends.
    pub fn scatter_traffic(&mut self, first: usize, margin: i32) {
        let track_dir = self.track().direction;
        for i in first..self.opp.opponents_used {
            let (chunk, lane, direction) = loop {
                let direction = if self.rand() % 4 == 0 { (track_dir == 0) as i32 } else { track_dir };
                let chunk = margin + self.rand() % (self.weg.chunk_count - margin * 3);
                let r = self.rand();
                let lane = if direction == 1 { r % 2 + 2 } else { r % 2 + 7 };
                if self.get_map_occupant(chunk, lane) == -1 {
                    break (chunk, lane, direction);
                }
            };
            self.set_map_occupant(i as i32, chunk, lane);
            let o = &mut self.opp.data[i];
            o.chunk = chunk;
            o.waypoint = chunk;
            o.target_lane = lane;
            o.lane = lane;
            o.direction = direction;
            // One unit back along the road, as the original.
            self.lane_position(i, chunk, lane, 1.0);
            let o = &mut self.opp.data[i];
            o.target_x = o.x;
            o.target_y = o.y;
            o.target_z = o.z;
            o.speed = 0.0;
            o.prev_waypoint = if direction == 0 { chunk + 1 } else { chunk - 1 };
            let (driver, kind) = (o.field_e0, o.field_e4);
            if driver != -1 {
                self.assign_driver_to_opponent(driver as usize, i, kind);
            }
            let heading = self.weg.sections[chunk as usize].heading + if direction == 0 { 0x100 } else { -0x100 };
            let o = &mut self.opp.data[i];
            o.heading = heading;
            o.field_18 = 0.0;
            o.field_1c = 0;
            o.field_20 = 0.0;
            o.heading_delta = 0;
        }
    }

    pub fn mark_active_racers(&mut self) {
        for o in &self.opp.data {
            let ob = o.object as usize;
            if o.active && ob < self.pools.objects3d.len() && self.pools.objects3d[ob].in_use && ob != self.player_index {
                self.pools.objects3d[ob].update_interval = 1;
            }
        }
    }

    // --- the active list (active.cpp) ---------------------------------------------------

    pub fn init_active_opponent_list(&mut self) {
        self.opp.active_list.clear();
        self.opp.nearby_list.clear();
    }

    /// The six racers always; other cars nearer than `active` chunks to the player move,
    /// nearer than `nearby` can be hit.
    fn update_active_opponent_list(&mut self, active: i32, nearby: i32) {
        let player = self.opp.data[0].chunk;
        let (mut a, mut n) = (Vec::new(), Vec::new());
        for i in 0..6.min(self.opp.count()) {
            a.push(i);
            n.push(i);
        }
        for (i, o) in self.opp.data.iter().enumerate().skip(6) {
            if !o.active {
                continue;
            }
            let d = (o.chunk - player).abs();
            if d < active {
                a.push(i);
            } else if nearby <= d {
                continue;
            }
            n.push(i);
        }
        self.opp.active_list = a;
        self.opp.nearby_list = n;
    }

    // --- collisions (objio.cpp) -------------------------------------------------------

    /// racers_collide: 0 apart, 1 within each other's radius, 2 overlapping (then, with
    /// `resolve`, pushed apart).
    fn racers_collide(&mut self, object_a: usize, object_b: usize, resolve: bool) -> i32 {
        let a = &self.opp.data[self.pools.objects3d[object_a].opponent as usize];
        let b = &self.opp.data[self.pools.objects3d[object_b].opponent as usize];
        let (dx, dz) = (b.x - (a.x + a.offset_x), b.z - (a.z + a.offset_z));
        let r = a.radius + b.radius;
        if dz * dz + dx * dx > r * r {
            return 0;
        }
        let mut pa = [[0.0f32; 3]; 5];
        let mut pb = [[0.0f32; 3]; 5];
        for i in 0..4 {
            pa[i] = [a.world_corners[i * 3] + a.offset_x, 0.0, a.world_corners[i * 3 + 2] + a.offset_z];
            pb[i] = [b.world_corners[i * 3], 0.0, b.world_corners[i * 3 + 2]];
        }
        pa[4] = [a.x + a.offset_x, 0.0, a.z + a.offset_z];
        pb[4] = [b.x, 0.0, b.z];
        if points_in_quad(&pb, &pa) || points_in_quad(&pa, &pb) {
            if resolve {
                self.resolve_collision(object_a, object_b);
            }
            return 2;
        }
        1
    }

    /// The two cars exchange momentum by mass; then A's move is cut short until clear.
    fn resolve_collision(&mut self, object_a: usize, object_b: usize) {
        let ia = self.pools.objects3d[object_a].opponent as usize;
        let ib = self.pools.objects3d[object_b].opponent as usize;
        let (a, b) = (self.opp.data[ia].clone(), self.opp.data[ib].clone());
        let (ax, az, m) = (a.offset_x, a.offset_z, a.mass + b.mass);
        let cx = (b.mass * b.offset_x + a.mass * ax) / m;
        let cz = (b.mass * b.offset_z + a.mass * az) / m;
        let (pax, paz) = ((cx - ax) + cx, (cz - az) + cz);
        let (pbx, pbz) = ((cx - b.offset_x) + cx, (cz - b.offset_z) + cz);
        let push = |o: &mut Opponent, px: f32, pz: f32| {
            o.push_x += px * 0.6;
            o.push_z += pz * 0.6;
            if 10.0 < o.push_x.abs() || 10.0 < o.push_z.abs() {
                o.push_x -= px * 0.6;
                o.push_z -= pz * 0.6;
            }
        };
        push(&mut self.opp.data[ia], pax, paz);
        push(&mut self.opp.data[ib], pbx, pbz);
        if self.racers_collide(object_a, object_b, false) != 0 {
            let mut f = 0.1f32;
            while f < 1.0 {
                self.opp.data[ia].offset_x = f * ax;
                self.opp.data[ia].offset_z = f * az;
                if self.racers_collide(object_a, object_b, false) != 0 {
                    break;
                }
                f += 0.1;
            }
            let a = &mut self.opp.data[ia];
            a.offset_x = (f - 0.1) * ax;
            a.offset_z = (f - 0.1) * az;
            a.speed *= 0.7;
            a.push_x *= 0.8;
            a.push_z *= 0.8;
            let (x, y, z, speed) = (a.x, a.y, a.z, a.speed);
            self.play_crash_sound(x, y, z, speed);
        }
    }

    fn check_opponent_collisions(&mut self, object: usize, op: usize) -> bool {
        for i in 0..self.opp.nearby_list.len() {
            let k = self.opp.nearby_list[i];
            if k == op {
                continue;
            }
            if (self.opp.data[k].chunk - self.opp.data[op].chunk).abs() < 2 {
                self.update_racer_corners(k);
                let other = self.opp.data[k].object as usize;
                if self.racers_collide(object, other, false) != 0 {
                    return true;
                }
            }
        }
        false
    }

    // --- every frame ---------------------------------------------------------------------

    fn update_all_opponents(&mut self) {
        let active = self.opp.active_list.clone();
        for &op in active.iter().skip(1) {
            let o = &self.opp.data[op];
            if o.car_type != 0x10 && (self.race_running || 0xd < op) {
                let driver = o.field_e0;
                self.drive_opponent(driver as usize);
            }
        }
        for j in 1..active.len() {
            // The original checks the car type of opponent j, not of the j-th active car.
            if self.opp.data[j].car_type == 0x10 {
                continue;
            }
            let op = active[j];
            let (chunk, lane) = (self.opp.data[op].chunk, self.opp.data[op].lane);
            self.clear_map_occupant(chunk, lane);
            self.evaluate_opponent(op);
            let o = &self.opp.data[op];
            let chunk = self.update_chunk_for_position(o.chunk, o.x, o.z);
            self.opp.data[op].chunk = chunk;
            self.next_opponent_waypoint(op, 2);
            let lane = self.get_racer_lane(op);
            self.opp.data[op].lane = lane;
            let chunk = self.opp.data[op].chunk;
            self.set_map_occupant(op as i32, chunk, lane);
            if 0 < op && op < 6 {
                // Rivals keep near the player: slower when far ahead, faster behind.
                let driver = self.opp.data[op].field_e0 as usize;
                let player_place = self.get_racer_entry(0);
                let place = self.get_racer_entry(op);
                if place < player_place {
                    if self.drivers.racer_range < (self.opp.data[op].chunk - self.opp.data[0].chunk).abs() {
                        self.drivers.driver_list[driver].speed = self.drivers.racer_low_speed;
                    }
                } else if player_place < place {
                    self.drivers.driver_list[driver].speed = self.drivers.racer_high_speed;
                }
                let (o_dir, o_chunk) = (self.opp.data[op].direction, self.opp.data[op].chunk);
                let count = self.weg.chunk_count;
                let over = if o_dir == 1 && count - 0xf <= o_chunk {
                    o_chunk - (count - 0xf)
                } else if o_dir != 1 && o_chunk < 0x10 {
                    0xf - o_chunk
                } else {
                    -1
                };
                if over >= 0 {
                    self.drivers.rival_states[op - 1].counter = 10 - over;
                    self.set_racer_finish_time(op);
                    self.drivers.rival_states[op - 1].state = 4;
                }
            }
        }
    }

    /// Moves every car one frame; the player first (update_racers).
    pub fn update_racers(&mut self) {
        let heading = self.player_heading;
        let pl = self.pools.objects3d[self.player_index].opponent as usize;
        let (chunk, lane) = (self.opp.data[pl].chunk, self.opp.data[pl].lane);
        self.clear_map_occupant(chunk, lane);
        self.handle_player_controls();
        let p = &mut self.opp.data[pl];
        p.x = self.player_x;
        p.y = self.player_y;
        p.z = self.player_z;
        p.heading = heading;
        p.heading_delta = self.player_heading - heading;
        p.speed = self.player_speed;
        p.chunk = self.player_chunk;
        let lane = self.get_racer_lane(pl);
        self.opp.data[pl].lane = lane;
        self.set_map_occupant(pl as i32, self.player_chunk, lane);
        self.update_active_opponent_list(10, 0xc);
        self.update_all_opponents();

        // Each active car turns and gets its move for this frame.
        let active = self.opp.active_list.clone();
        for &op in &active {
            let o = &mut self.opp.data[op];
            o.heading += o.heading_delta;
            self.update_racer_corners(op);
            let o = &self.opp.data[op];
            if o.heading_delta != 0 && (op == pl || (o.chunk - self.opp.data[pl].chunk).abs() <= 1) {
                let o = &mut self.opp.data[op];
                o.offset_x = 0.0;
                o.offset_z = 0.0;
                if o.speed < 4.0 {
                    // Slow cars may not turn into another car: undo the turn in quarters.
                    let was = o.heading;
                    let object = o.object as usize;
                    let mut k = 4;
                    loop {
                        k -= 1;
                        if k == -1 || !self.check_opponent_collisions(object, op) {
                            break;
                        }
                        let o = &mut self.opp.data[op];
                        o.heading -= o.heading_delta / 4;
                        self.update_racer_corners(op);
                    }
                    if k == 0 {
                        self.opp.data[op].heading = was;
                        self.update_racer_corners(op);
                    }
                    self.opp.data[op].heading_delta = 0;
                }
            }
            let o = &mut self.opp.data[op];
            o.field_18 = cos_a(o.heading + 0x100);
            o.field_1c = 0;
            o.field_20 = sin_a(o.heading + 0x100);
            o.offset_x = o.field_18 * o.speed + o.push_x;
            o.offset_z = o.field_20 * o.speed + o.push_z;
            self.collide_racer_with_road_edge(op);
            if op == 0 {
                self.collide_player_with_track_ends();
            }
        }
        // Then everyone moves, pushing apart what they run into.
        for &op in &active {
            let o = &self.opp.data[op];
            let object = o.object as usize;
            if !self.pools.objects3d[object].in_use || !o.active {
                continue;
            }
            self.update_racer_corners(op);
            for j in 0..self.opp.nearby_list.len() {
                let k = self.opp.nearby_list[j];
                if k != op && self.opp.data[op].chunk - self.opp.data[k].chunk < 2 {
                    self.update_racer_corners(k);
                    let other = self.opp.data[k].object as usize;
                    self.racers_collide(object, other, true);
                }
            }
            let o = &mut self.opp.data[op];
            o.x += o.offset_x;
            o.z += o.offset_z;
            self.update_racer_corners(op);
        }
        let p = &mut self.opp.data[pl];
        p.push_x *= 0.75;
        p.push_z *= 0.75;
        self.player_x = p.x;
        self.player_z = p.z;
        self.player_y = p.y;
        self.player_speed = p.speed;
        self.player_heading = p.heading;
        self.player.select_rpm_step_for_speed(self.player_speed);
        self.update_camera();
    }
}
