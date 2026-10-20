//! Pionnen (pion.cpp): the traffic cones of the road works, knocked flying by the
//! player, the rivals and the police.

use crate::race::driver::{DRIVER_POLICE, DRIVER_RIVAL};
use crate::race::object3d::Update;
use crate::race::opponent::LANE_OFFSETS;
use crate::race::Race;

/// The static object types: the cone and the road works sign.
const PION_OBJECT_LEFT: usize = 0x13; // werkweg.d3d
const PION_OBJECT_RIGHT: usize = 0x14; // werkweg2.d3d

/// 0x2c bytes in the original.
#[derive(Clone, Copy)]
struct Pion {
    in_use: bool,
    /// Knocked over, flying.
    hit: bool,
    chunk: i32,
    lane: i32,
    object: usize,
}

const EMPTY_PION: Pion = Pion { in_use: false, hit: false, chunk: -1, lane: -1, object: 0 };

#[derive(Default)]
pub struct Pions {
    pionnen: Vec<Pion>,
}

impl Race {
    /// 60 pionnen (init_pionnen).
    pub fn init_pionnen(&mut self) {
        self.pions.pionnen = vec![EMPTY_PION; 0x3c];
    }

    /// Moves by its velocity and tumbles by its three angles; like a bonus item's, the
    /// matrix keeps the turns so far.
    pub fn update_pion_object(&mut self, object: usize) {
        let o = &mut self.pools.objects3d[object];
        o.x += o.vx;
        o.y += o.vy;
        o.z += o.vz;
        let mut m = o.matrix;
        m.m[12] = o.x;
        m.m[13] = o.y;
        m.m[14] = o.z;
        m.m[15] = 1.0;
        if o.field_3f != 0 {
            m.rotate_y(o.field_3f);
        }
        if o.field_3b != 0 {
            m.rotate_x(o.field_3b);
        }
        if o.field_43 != 0 {
            m.rotate_z(o.field_43);
        }
        o.matrix = m;
    }

    fn load_pion_object(&mut self, ty: usize, x: f32, z: f32, y: i32) -> usize {
        let object = self.new_static_object(ty);
        let o = &mut self.pools.objects3d[object];
        o.update = Update::Pion;
        o.x = x;
        o.y = y as f32 + 0.1;
        o.z = z;
        o.field_3f = 0;
        o.field_3b = 0;
        o.field_43 = 0;
        o.in_use = true;
        self.build_object_execute_buffer(object);
        object
    }

    /// place_pion_left and place_pion_right differ only in the object type.
    fn place_pion(&mut self, who: &str, ty: usize, chunk: i32, lane: i32) {
        assert!((0..=self.weg.chunk_count).contains(&chunk), "{who}: chunk value out of bounds");
        assert!((0..=0xb).contains(&lane), "{who}: lane value out of bounds");
        let i = self.pions.pionnen.iter().position(|p| !p.in_use).unwrap_or_else(|| panic!("{who}: no more free pion objects"));
        let c = &self.weg.chunks[chunk as usize];
        let centre = self.pools.vertices3d[self.weg.sections[chunk as usize].point];
        let l = LANE_OFFSETS[lane as usize];
        let x = c.side[0] * l + centre.x - c.side[2] * 40.0;
        let z = c.side[0] * 40.0 + c.side[2] * l + centre.z;
        let y = self.road_height_at(chunk, x, z) as i32;
        let object = self.load_pion_object(ty, x, z, y);
        self.pions.pionnen[i] = Pion { in_use: true, hit: false, chunk, lane, object };
    }

    pub fn place_pion_left(&mut self, chunk: i32, lane: i32) {
        self.place_pion("place_pion_left", PION_OBJECT_LEFT, chunk, lane);
    }

    pub fn place_pion_right(&mut self, chunk: i32, lane: i32) {
        self.place_pion("place_pion_right", PION_OBJECT_RIGHT, chunk, lane);
    }

    /// Flying cones fall; cars hit the ones in their chunk and lane.
    pub fn collide_with_pionnen(&mut self, opponent: usize) {
        assert!(!self.pions.pionnen.is_empty(), "collide_with_pionnen: no pion objects allocated");
        let op = self.opp.data[opponent].clone();
        if op.chunk == -1 || op.lane == -1 {
            return;
        }
        for i in 0..self.pions.pionnen.len() {
            let p = self.pions.pionnen[i];
            if !p.in_use {
                continue;
            }
            if p.hit {
                let r = [self.rand() & 0xf, self.rand() & 0xf, self.rand() & 0xf];
                let o = &mut self.pools.objects3d[p.object];
                o.vy += -0.125;
                o.vx *= 0.95;
                o.vz *= 0.95;
                o.field_3f = (o.field_3f + r[0]) % 0x400;
                o.field_3b = (o.field_3b + r[1]) % 0x400;
                o.field_43 = (o.field_43 + r[2]) % 0x400;
            }
            if self.pools.objects3d[p.object].y <= 0.0 {
                let o = &mut self.pools.objects3d[p.object];
                o.update_interval = -1;
                o.visible = false;
                o.in_use = false;
                self.pions.pionnen[i] = EMPTY_PION;
            }
            let p = self.pions.pionnen[i];
            if p.chunk == op.chunk && p.lane == op.lane {
                let o = &mut self.pools.objects3d[p.object];
                o.vx = op.field_18 * op.speed;
                o.vz = op.field_20 * op.speed;
                o.vy = (o.vz.abs() + o.vx.abs()) / 3.0;
                let (x, y, z) = (o.x, o.y, o.z);
                if !p.hit {
                    self.play_bump_sound(x, y, z);
                }
                self.pions.pionnen[i].hit = true;
                break;
            }
        }
    }

    /// A sign and ten cones on the outer lanes from the chunk on.
    pub fn place_pion_group(&mut self, chunk: i32) {
        let direction = self.track().direction;
        let n = self.weg.section_count;
        let (c, lane) = if direction == 1 { ((chunk - 1).max(0), 0) } else { ((chunk + 1).min(n - 1), 10) };
        self.place_pion_right(c, lane);
        for i in 0..10 {
            let (c, lane) = if direction == 1 {
                (chunk + i, if self.rand() % 2 != 0 { 1 } else { 0 })
            } else {
                (chunk - i, if self.rand() % 2 == 0 { 10 } else { 9 })
            };
            self.place_pion_left(c.max(0).min(n - 1), lane);
        }
    }

    /// setracer.cpp: four or five groups along the road.
    pub fn place_random_pionnen(&mut self) {
        let groups = self.rand() % 2 + 4;
        let step = self.weg.chunk_count / 0x14;
        let mut along = 0;
        for _ in 0..groups {
            let chunk = step as f64 + along as f64 * 1.5;
            along += step;
            self.place_pion_group(chunk as i32);
        }
    }

    pub fn update_pionnen(&mut self) {
        self.collide_with_pionnen(0);
        for i in 1..self.opp.opponents_used {
            let kind = self.opp.data[i].field_e4;
            if kind == DRIVER_RIVAL || kind == DRIVER_POLICE {
                self.collide_with_pionnen(i);
            }
        }
    }
}
