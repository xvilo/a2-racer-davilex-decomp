//! Bonus items (bonus.cpp): spinning time, fuel and money pickups on the road. Driving
//! over one (same chunk and lane) collects it.

use crate::race::object3d::Update;
use crate::race::opponent::LANE_OFFSETS;
use crate::race::Race;

pub const BONUS_NONE: i32 = 0;
pub const BONUS_TIME: i32 = 1;
pub const BONUS_FUEL: i32 = 2;
pub const BONUS_MONEY: i32 = 3;

/// The static object types of the pickups.
const BONUS_OBJECT_MONEY: usize = 0x18;
const BONUS_OBJECT_FUEL: usize = 0x19;
const BONUS_OBJECT_TIME: usize = 0x1a;

/// The original packs these in 25 bytes.
#[derive(Clone, Copy)]
struct BonusItem {
    x: f32,
    #[allow(dead_code)]
    y: f32,
    z: f32,
    /// BONUS_*, 0 for a free entry.
    ty: i32,
    chunk: i32,
    lane: i32,
    object: i32,
}

const EMPTY_BONUS: BonusItem = BonusItem { x: 0.0, y: 0.0, z: 0.0, ty: BONUS_NONE, chunk: -1, lane: -1, object: -1 };

pub struct Bonus {
    items: Vec<BonusItem>,
    /// What was last collected (unused by the original once written).
    #[allow(dead_code)]
    status: &'static str,
    /// Seconds; BONUS/TIME minutes in A2.INI replace it.
    pub time_bonus: i32,
}

impl Default for Bonus {
    fn default() -> Bonus {
        Bonus { items: Vec::new(), status: "None", time_bonus: 300 }
    }
}

impl Race {
    pub fn bonus_add_time(&mut self) {
        self.time_left += self.bonus.time_bonus;
        self.play_sound_effect(0x1c, false);
    }

    pub fn bonus_add_fuel(&mut self) {
        if self.fuel != 0 {
            self.fuel += 0x4b;
            if 299 < self.fuel {
                self.fuel = 300;
            }
        }
        self.play_sound_effect(0x1d, false);
    }

    pub fn bonus_add_money(&mut self) {
        self.money += 0xfa;
        if 0x270e < self.money {
            self.money = 9999;
        }
        self.play_sound_effect(0x1e, false);
    }

    /// Turns a little further every frame; the matrix keeps the turns so far, so the item
    /// spins faster and slower as field_3f goes round.
    pub fn update_bonus_object(&mut self, object: usize) {
        let o = &mut self.pools.objects3d[object];
        let mut m = o.matrix;
        m.m[12] = o.x;
        m.m[13] = o.y;
        m.m[14] = o.z;
        m.m[15] = 1.0;
        m.rotate_y(o.field_3f);
        o.matrix = m;
        o.field_3f = (o.field_3f + 0xf) % 0x400;
    }

    fn load_bonus_object(&mut self, ty: usize, x: f32, z: f32, y: i32, heading: i32) -> usize {
        let object = self.new_static_object(ty);
        let o = &mut self.pools.objects3d[object];
        o.update = Update::Bonus;
        o.x = x;
        o.y = y as f32;
        o.z = z;
        o.field_3f = heading;
        o.in_use = true;
        self.build_object_execute_buffer(object);
        object
    }

    /// A bonus of the type at the chunk, in the lane (0..10).
    pub fn place_bonus_item(&mut self, ty: i32, chunk: i32, lane: i32) {
        assert!(ty != BONUS_NONE, "place_bonus_item: cannot set a bonus object of type none");
        assert!((0..=self.weg.chunk_count).contains(&chunk), "place_bonus_item: chunk value {chunk} out of bounds");
        assert!((0..=0xb).contains(&lane), "place_bonus_item: lane value {lane} out of bounds");
        let object_type = match ty {
            BONUS_TIME => BONUS_OBJECT_TIME,
            BONUS_FUEL => BONUS_OBJECT_FUEL,
            BONUS_MONEY => BONUS_OBJECT_MONEY,
            _ => panic!("place_bonus_item: illegal bonus object type"),
        };
        let i = self.bonus.items.iter().position(|b| b.ty == BONUS_NONE).expect("place_bonus_item: no more free bonus objects");
        let c = &self.weg.chunks[chunk as usize];
        let centre = self.pools.vertices3d[self.weg.sections[chunk as usize].point];
        let l = LANE_OFFSETS[lane as usize];
        let x = c.side[0] * l + centre.x - c.side[2] * 40.0;
        let z = c.side[0] * 40.0 + c.side[2] * l + centre.z;
        let heading = 0x100 - self.weg.sections[chunk as usize].heading;
        let y = self.road_height_at(chunk, x, z) as i32;
        let object = self.load_bonus_object(object_type, x, z, y, heading) as i32;
        self.bonus.items[i] = BonusItem { x, y: 0.0, z, ty, chunk, lane, object };
    }

    /// The bonus the racer is on, now taken off the road, or BONUS_NONE.
    pub fn describe_bonus(&mut self, opponent: usize) -> i32 {
        assert!(!self.bonus.items.is_empty(), "describe_bonus: no bonus objects allocated");
        let o = &self.opp.data[opponent];
        if o.chunk == -1 || o.lane == -1 {
            return BONUS_NONE;
        }
        let (chunk, lane) = (o.chunk, o.lane);
        let Some(b) = self.bonus.items.iter_mut().find(|b| b.ty != BONUS_NONE && b.chunk == chunk && b.lane == lane) else {
            return BONUS_NONE;
        };
        let (ty, object) = (b.ty, b.object as usize);
        *b = EMPTY_BONUS;
        let ob = &mut self.pools.objects3d[object];
        ob.update_interval = -1;
        ob.visible = false;
        ob.in_use = false;
        ty
    }

    /// One per 27 chunks, spread along the road.
    pub fn place_all_bonus_items(&mut self) {
        let count = self.weg.chunk_count / 0x1b;
        self.bonus.items = vec![EMPTY_BONUS; count.max(0) as usize];
        if count <= 0 {
            return;
        }
        let mut last_chunk = 0x14;
        let spacing = (self.weg.chunk_count - 0x28) / count;
        let mut base = 0x14;
        for _ in 0..count {
            let chunk = base - self.rand() % 10;
            let mut lane = self.rand() % 4;
            if lane == 3 {
                lane = 2;
            }
            lane += 1;
            if self.track().direction == 0 {
                lane = 10 - lane;
            }
            let mut ty = self.rand() % 4;
            if ty == 0 {
                ty = BONUS_MONEY;
            }
            // Fuel at least every 70 chunks: past that, the item becomes fuel.
            if ty == BONUS_FUEL {
                last_chunk = chunk;
            } else if 0x46 < (chunk - last_chunk).abs() {
                ty = BONUS_FUEL;
                last_chunk = chunk;
            }
            self.place_bonus_item(ty, chunk, lane);
            base += spacing;
        }
    }

    /// Collects the bonus the player is on.
    pub fn draw_bonus_status(&mut self) {
        match self.describe_bonus(0) {
            BONUS_TIME => {
                self.bonus.status = "Time";
                self.bonus_add_time();
            }
            BONUS_FUEL => {
                self.bonus.status = "Fuel";
                self.bonus_add_fuel();
            }
            BONUS_MONEY => {
                self.bonus.status = "Money";
                self.bonus_add_money();
            }
            _ => {}
        }
    }
}
