//! Structures saved to disk by the menu, read and written byte for byte as in
//! A2MENU.DAT, so the port uses the original's OPTIE.A2R, BEDIEN.A2R and SLOT.A2R.

pub const SOUND_ALL: i16 = 0; // music and effects
pub const SOUND_EFFECTS_ONLY: i16 = 1;
pub const SOUND_OFF: i16 = 2;
pub const SOUND_MUSIC_ONLY: i16 = 3;

pub const SLOT_COUNT: usize = 10;
pub const STAGE_COUNT: usize = 6;

/// Little-endian reading and writing of the packed records.
pub struct Reader<'a> {
    pub p: &'a [u8],
}

impl Reader<'_> {
    pub fn u8(&mut self) -> u8 {
        let v = self.p[0];
        self.p = &self.p[1..];
        v
    }
    pub fn i16(&mut self) -> i16 {
        let v = i16::from_le_bytes([self.p[0], self.p[1]]);
        self.p = &self.p[2..];
        v
    }
    pub fn i32(&mut self) -> i32 {
        let v = i32::from_le_bytes([self.p[0], self.p[1], self.p[2], self.p[3]]);
        self.p = &self.p[4..];
        v
    }
    pub fn bytes<const N: usize>(&mut self) -> [u8; N] {
        let mut out = [0u8; N];
        out.copy_from_slice(&self.p[..N]);
        self.p = &self.p[N..];
        out
    }
}

/// OPTIE.A2R, and each save slot's copy (0x14 bytes).
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Opties {
    pub view: i16,
    pub resolution: i16,
    /// SOUND_*.
    pub sound: i16,
    /// 0 3D accelerator, 1 320x200, 2 640x480.
    pub display_mode: i16,
    pub horizon: i16,
    pub dashboard: i16,
    pub detail: i16,
    pub camera: i16,
    /// 0..4.
    pub music_volume: i16,
    /// 0..4.
    pub effects_volume: i16,
}

impl Opties {
    pub const SIZE: usize = 0x14;

    pub fn read(r: &mut Reader) -> Opties {
        Opties {
            view: r.i16(),
            resolution: r.i16(),
            sound: r.i16(),
            display_mode: r.i16(),
            horizon: r.i16(),
            dashboard: r.i16(),
            detail: r.i16(),
            camera: r.i16(),
            music_volume: r.i16(),
            effects_volume: r.i16(),
        }
    }

    pub fn write(&self, out: &mut Vec<u8>) {
        for v in [self.view, self.resolution, self.sound, self.display_mode, self.horizon, self.dashboard, self.detail,
            self.camera, self.music_volume, self.effects_volume]
        {
            out.extend_from_slice(&v.to_le_bytes());
        }
    }
}

/// BEDIEN.A2R: controls (0x24 bytes). Keys are Windows virtual-key codes.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Bedien {
    /// 0 keyboard, 1 joypad, 2 steering wheel.
    pub control_type: i32,
    pub field_04: i32,
    pub field_08: i32,
    pub key_left: i32,
    pub key_right: i32,
    pub key_gas: i32,
    pub key_brake: i32,
    pub key_action1: i32,
    pub key_action2: i32,
}

impl Bedien {
    pub const SIZE: usize = 0x24;

    pub fn read(r: &mut Reader) -> Bedien {
        Bedien {
            control_type: r.i32(),
            field_04: r.i32(),
            field_08: r.i32(),
            key_left: r.i32(),
            key_right: r.i32(),
            key_gas: r.i32(),
            key_brake: r.i32(),
            key_action1: r.i32(),
            key_action2: r.i32(),
        }
    }

    pub fn write(&self, out: &mut Vec<u8>) {
        for v in [self.control_type, self.field_04, self.field_08, self.key_left, self.key_right, self.key_gas,
            self.key_brake, self.key_action1, self.key_action2]
        {
            out.extend_from_slice(&v.to_le_bytes());
        }
    }

    /// The six assignable keys in order (left, right, gas, brake, action 1 and 2).
    pub fn keys(&self) -> [i32; 6] {
        [self.key_left, self.key_right, self.key_gas, self.key_brake, self.key_action1, self.key_action2]
    }

    pub fn key_mut(&mut self, i: usize) -> &mut i32 {
        match i {
            0 => &mut self.key_left,
            1 => &mut self.key_right,
            2 => &mut self.key_gas,
            3 => &mut self.key_brake,
            4 => &mut self.key_action1,
            _ => &mut self.key_action2,
        }
    }
}

/// SLOT.A2R holds SLOT_COUNT of these (0x13c bytes each).
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Slot {
    pub used: u8,
    /// The highest is the most recently saved.
    pub save_order: u8,
    pub opties: Opties,
    pub control: Bedien,
    pub name: [u8; 32],
    pub field_5a: i16,
    pub field_5c: i16,
    /// Current stage, 0..5.
    pub stage: i16,
    /// The player's time per stage, seconds.
    pub time: [i32; STAGE_COUNT],
    /// 1000 at the start.
    pub money: i32,
    /// 6: not raced.
    pub position: [i16; STAGE_COUNT],
    pub time_racer: [[i32; STAGE_COUNT]; 5],
    pub racer: [[i16; STAGE_COUNT]; 5],
}

impl Default for Slot {
    fn default() -> Slot {
        Slot {
            used: 0,
            save_order: 0,
            opties: Opties::default(),
            control: Bedien::default(),
            name: [0; 32],
            field_5a: 0,
            field_5c: 0,
            stage: 0,
            time: [0; STAGE_COUNT],
            money: 0,
            position: [0; STAGE_COUNT],
            time_racer: [[0; STAGE_COUNT]; 5],
            racer: [[0; STAGE_COUNT]; 5],
        }
    }
}

impl Slot {
    pub const SIZE: usize = 0x13c;

    pub fn read(r: &mut Reader) -> Slot {
        let mut s = Slot {
            used: r.u8(),
            save_order: r.u8(),
            opties: Opties::read(r),
            control: Bedien::read(r),
            name: r.bytes::<32>(),
            field_5a: r.i16(),
            field_5c: r.i16(),
            stage: r.i16(),
            ..Slot::default()
        };
        for t in s.time.iter_mut() {
            *t = r.i32();
        }
        s.money = r.i32();
        for p in s.position.iter_mut() {
            *p = r.i16();
        }
        for row in s.time_racer.iter_mut() {
            for t in row.iter_mut() {
                *t = r.i32();
            }
        }
        for row in s.racer.iter_mut() {
            for v in row.iter_mut() {
                *v = r.i16();
            }
        }
        s
    }

    pub fn write(&self, out: &mut Vec<u8>) {
        out.push(self.used);
        out.push(self.save_order);
        self.opties.write(out);
        self.control.write(out);
        out.extend_from_slice(&self.name);
        for v in [self.field_5a, self.field_5c, self.stage] {
            out.extend_from_slice(&v.to_le_bytes());
        }
        for t in self.time {
            out.extend_from_slice(&t.to_le_bytes());
        }
        out.extend_from_slice(&self.money.to_le_bytes());
        for p in self.position {
            out.extend_from_slice(&p.to_le_bytes());
        }
        for row in self.time_racer {
            for t in row {
                out.extend_from_slice(&t.to_le_bytes());
            }
        }
        for row in self.racer {
            for v in row {
                out.extend_from_slice(&v.to_le_bytes());
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn record_sizes_match_the_original() {
        let mut out = Vec::new();
        Opties::default().write(&mut out);
        assert_eq!(out.len(), Opties::SIZE);
        out.clear();
        Bedien::default().write(&mut out);
        assert_eq!(out.len(), Bedien::SIZE);
        out.clear();
        let mut s = Slot::default();
        s.time_racer[4][5] = 0x01020304;
        s.write(&mut out);
        assert_eq!(out.len(), Slot::SIZE);
        assert_eq!(Slot::read(&mut Reader { p: &out }), s);
    }
}
