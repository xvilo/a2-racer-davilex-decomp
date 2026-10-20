//! Scenery (statobj.cpp, mkstobj.cpp): buildings, signs, trees, the sky and the
//! position boards, each one of 48 static object types with its model and texture.

use crate::race::math3d::{normalize_vector, Matrix};
use crate::race::object3d::Update;
use crate::race::Race;

pub const STATIC_OBJECT_TYPES: usize = 48;

/// static_object_types in A2SPEL.DAT: model and texture.
const STATIC_OBJECT_FILES: [(&str, &str); STATIC_OBJECT_TYPES] = [
    ("sky.d3d", "sky.pcx"), ("bovw.d3d", "50.pcx"), ("arena.d3d", "arena.pcx"),
    ("viaduct.d3d", "viaduct.pcx"), ("molent.d3d", "molen.pcx"), ("shell.d3d", "shell.pcx"),
    ("roadsgn.d3d", "roadsgn.pcx"), ("uit.d3d", "uit.pcx"), ("zaltboml.d3d", "zaltboml.pcx"),
    ("lekbrug.d3d", "lekbrug.pcx"), ("gebouwg.d3d", "gebouw.pcx"), ("gebouw2g.d3d", "tile.pcx"),
    ("gebouw3g.d3d", "tile2.pcx"), ("trees.d3d", "trees.pcx"), ("wegresg2.d3d", "wegresg2.pcx"),
    ("elek4.d3d", "elek4.pcx"), ("huisje.d3d", "huisje.pcx"), ("rijtjes.d3d", "rijtjes.pcx"),
    ("werkweg.d3d", "werkweg.pcx"), ("werkweg2.d3d", "werkweg2.pcx"), ("flits.d3d", "flits.pcx"),
    ("praatp.d3d", "praatp.pcx"), ("start.d3d", "start.pcx"), ("finish.d3d", "finish.pcx"),
    ("geld.d3d", "geld.pcx"), ("benzine.d3d", "benzine.pcx"), ("tijd.d3d", "tijd.pcx"),
    ("lantaarn.d3d", "lantaarn.pcx"), ("eindh.d3d", "eindh.pcx"), ("amsterd.d3d", "amsterd.pcx"),
    ("denbosch.d3d", "denbosch.pcx"), ("maastr.d3d", "maastr.pcx"), ("utrecht.d3d", "utrecht.pcx"),
    ("s_maastr.d3d", "s_maastr.pcx"), ("s_utr.d3d", "s_utr.pcx"), ("s_denb.d3d", "s_denb.pcx"),
    ("s_adam.d3d", "s_adam.pcx"), ("s_eindho.d3d", "s_eindho.pcx"), ("number1.d3d", "number1.pcx"),
    ("number2.d3d", "number2.pcx"), ("number3.d3d", "number3.pcx"), ("number4.d3d", "number4.pcx"),
    ("number5.d3d", "number5.pcx"), ("number6.d3d", "number6.pcx"), ("hek.d3d", "hek.pcx"),
    ("koe.d3d", "koe.pcx"), ("100.d3d", "100.pcx"), ("babe.d3d", "babe.pcx"),
];

/// ETAPPE1..3.OMP per stage; stages 4..6 drive the tracks back.
const OMP_FILES: [&str; 6] = ["ETAPPE1.OMP", "ETAPPE2.OMP", "ETAPPE3.OMP", "ETAPPE3.OMP", "ETAPPE2.OMP", "ETAPPE1.OMP"];

/// The loaded ids of a type: its texture and (once loaded) its model's frame.
#[derive(Clone, Copy)]
pub struct StaticObjectType {
    pub texture: i32,
    pub mesh: i32,
}

fn angle_of(y: f64, x: f64) -> i32 {
    ((y.atan2(x) as f32) as f64 * 1024.0 / 6.283185307) as i32
}

impl Race {
    pub fn load_static_object_textures(&mut self) {
        for i in 0..STATIC_OBJECT_TYPES {
            let t = self.render.textures.load_texture(&self.files.rcs, STATIC_OBJECT_FILES[i].1);
            self.static_object_types[i] = StaticObjectType { texture: t, mesh: -1 };
        }
    }

    /// A new object of the type, its model and texture loaded (shared by every object of
    /// the type), with an identity matrix; bonus.cpp and pion.cpp repeat this code.
    pub fn new_static_object(&mut self, ty: usize) -> usize {
        let object = self.pools.get_new_object3d();
        let anim = self.pools.init_anim3d(1);
        {
            let o = &mut self.pools.objects3d[object];
            o.in_use = true;
            o.anim = anim;
            o.frame = 0;
            o.update_interval = 1;
            o.field_36 = 0;
        }
        if self.static_object_types[ty].mesh == -1 {
            let name = STATIC_OBJECT_FILES[ty].0;
            let data = self.files.race_file(name).unwrap_or_else(|| panic!("Unable to load object {name}")).to_vec();
            self.pools.load_object_mesh(object, &data, name);
            self.static_object_types[ty].mesh = self.pools.object_frame_index(object);
        } else {
            let (a, f) = (anim as usize, 0);
            self.pools.anims3d[a].frames[f] = self.static_object_types[ty].mesh;
        }
        let tex = self.static_object_types[ty].texture;
        if tex == -1 {
            panic!("texture not loaded for static object {ty}");
        }
        self.pools.objects3d[object].texture = tex;
        self.pools.set_object3d_matrix(object, &Matrix::identity());
        object
    }

    /// Places the object once, turned to its heading (field_3f), then stops updating.
    pub fn update_static_object(&mut self, object: usize) {
        let o = &mut self.pools.objects3d[object];
        let mut m = o.matrix;
        m.m[12] = o.x;
        m.m[13] = o.y;
        m.m[14] = o.z;
        m.m[15] = 1.0;
        m.rotate_y(0x4ff - (o.field_3f & 0x3ff));
        o.matrix = m;
        o.update_interval = -1;
    }

    /// The sky dome stays centred on the player.
    pub fn update_sky_object(&mut self, object: usize) {
        let (px, pz) = (self.player_x, self.player_z);
        let o = &mut self.pools.objects3d[object];
        o.matrix.m[12] = px;
        o.matrix.m[14] = pz;
        o.matrix.m[15] = 1.0;
        o.matrix.m[13] = 0.0;
        o.visible = true;
        o.update_interval = 1;
    }

    /// The boards "1".."6" float above the racer in that place (types 0x26..0x2b).
    pub fn update_position_board(&mut self, object: usize, place: i32) {
        let Some(ranking) = self.compute_race_ranking() else { return };
        let Some(i) = ranking.iter().position(|&r| r == place) else { return };
        let op = &self.opp.data[i];
        let mut m = Matrix::identity();
        m.m[12] = op.x;
        m.m[13] = op.y + 10.0;
        m.m[14] = op.z;
        m.rotate_y(0x3ff - (op.heading & 0x3ff));
        let o = &mut self.pools.objects3d[object];
        o.matrix = m;
        o.visible = true;
        o.update_interval = 1;
    }

    /// A free-standing object of a type at (x, z), facing `heading` (the sky: type 0).
    pub fn load_static_object(&mut self, ty: usize, x: f32, z: f32, heading: i32) -> usize {
        let object = self.new_static_object(ty);
        let o = &mut self.pools.objects3d[object];
        o.update = match ty {
            0 => Update::Sky,
            0x26..=0x2b => Update::PositionBoard(ty as i32 - 0x25),
            _ => Update::StaticObject,
        };
        o.update_interval = 1;
        o.x = x;
        o.y = 0.0;
        o.z = z;
        o.field_3f = heading;
        o.in_use = true;
        self.build_object_execute_buffer(object);
        object
    }

    /// An object beside the road: at chunk, centred (position 0), on the road's left or
    /// right `offset` out (1, 2), or on the ground beyond (3, 4); `angle` 0 or 1 picks
    /// whether it faces along or across the road.
    pub fn place_static_object_on_road(&mut self, ty: usize, chunk: i32, position: i32, angle: i32, offset: i32) {
        let object = self.new_static_object(ty);
        let c = chunk as usize;
        let p = self.pools.vertices3d[self.weg.sections[self.weg.chunks[c].section].point];
        let op = self.alloc_opponent_slot();
        self.pools.objects3d[object].opponent = op as i32;
        self.pools.objects3d[object].update = Update::StaticObject;
        self.compute_object_bounds(object);
        let mut side = [self.weg.chunks[c].side[0], 0.0, self.weg.chunks[c].side[2]];
        normalize_vector(&mut side);
        let (d, radius) = (offset as f32, self.opp.data[op].radius);
        let (x, y, z) = match position {
            // The original reads an uninitialised x here; the centre point it is.
            0 => (p.x, self.road_height_at(chunk, p.x, p.z), p.z),
            1 => {
                let (x, z) = (p.x - side[0] * d, p.z - side[2] * d);
                (x, self.road_height_at(chunk, x, z), z)
            }
            2 => {
                let (x, z) = (side[0] * d + p.x, side[2] * d + p.z);
                (x, self.road_height_at(chunk, x, z), z)
            }
            3 => (p.x - (radius + d * 2.0) * side[0], 0.0, p.z - (d * 2.0 + radius) * side[2]),
            4 => ((radius + d * 2.0) * side[0] + p.x, 0.0, (d * 2.0 + radius) * side[2] + p.z),
            _ => {
                let o = &self.pools.objects3d[object];
                (o.x, o.y, o.z)
            }
        };
        // Facing: along the road (angle 0) or across it (1), turned to the road's side.
        let left = position == 1 || position == 3;
        let (s0, s2) = (side[0] as f64, side[2] as f64);
        let o = &mut self.pools.objects3d[object];
        o.x = x;
        o.y = y;
        o.z = z;
        if angle == 0 {
            o.field_3f = if !left {
                if side[0] == 0.0 { 0x200 } else { angle_of(-s2, -s0) }
            } else if side[0] == 0.0 {
                0
            } else {
                angle_of(s2, s0)
            };
        } else if angle == 1 {
            o.field_3f = if !left {
                if side[2] == 0.0 { 0x100 } else { angle_of(-s0, s2) }
            } else if side[2] == 0.0 {
                0x312
            } else {
                angle_of(s0, -s2)
            };
        }
        o.in_use = true;
        self.opp.data[op].active = false;
        self.build_object_execute_buffer(object);
    }

    /// ETAPPE1..3.OMP: the stage's scenery, 20-byte records (chunk, angle, position, id,
    /// offset).
    pub fn init_static_objects(&mut self, stage: usize) {
        assert!(stage <= 5, "illegal level {stage}");
        let data = self.files.race_disc_file(OMP_FILES[stage]).filter(|d| d.len() >= 4)
            .unwrap_or_else(|| panic!("could not open {}", OMP_FILES[stage]));
        let i32_at = |p: usize| i32::from_le_bytes([data[p], data[p + 1], data[p + 2], data[p + 3]]);
        let count = i32_at(0);
        for i in 0..count.max(0) as usize {
            let r = 4 + i * 20;
            if r + 20 > data.len() {
                break;
            }
            let (chunk, angle, position, id) = (i32_at(r), i32_at(r + 4), i32_at(r + 8), i32_at(r + 12));
            let offset = f32::from_le_bytes([data[r + 16], data[r + 17], data[r + 18], data[r + 19]]);
            if id >= 0 && (id as usize) < STATIC_OBJECT_TYPES && chunk >= 0 && position >= 0 && angle >= 0
                && chunk < self.weg.chunk_count
            {
                self.place_static_object_on_road(id as usize, chunk, position, angle, offset as i32);
            }
        }
    }
}
