//! Player save slots (slot.cpp), kept in SLOT.A2R.

use crate::menu::a2types::{Reader, Slot, SLOT_COUNT, STAGE_COUNT};
use crate::menu::menu_data::LEVEL_NAAM;
use crate::menu::menu_types::{cstr, name32};
use crate::menu::Menu;
use crate::xvga::fileio::{file_delete, file_exist, file_read, file_write, save_path};

pub fn slot_check_index(index: i32) -> usize {
    assert!((0..SLOT_COUNT as i32).contains(&index), "slot_check_index: unknown slot {index}");
    index as usize
}

pub fn slot_check_level(level: i32) -> usize {
    assert!((0..STAGE_COUNT as i32).contains(&level), "slot_check_level: unknown stage {level}");
    level as usize
}

/// Per-stage results of a slot that has not raced yet: rivals 1..5 in order.
fn clear_results(s: &mut Slot) {
    for i in 0..STAGE_COUNT {
        s.time[i] = 0;
        s.position[i] = 6;
        for r in 0..5 {
            s.racer[r][i] = r as i16 + 1;
            s.time_racer[r][i] = 0;
        }
    }
}

/// The SLOT.A2R bytes of all slots.
pub fn slots_bytes(slots: &[Slot; SLOT_COUNT]) -> Vec<u8> {
    let mut out = Vec::with_capacity(Slot::SIZE * SLOT_COUNT);
    for s in slots {
        s.write(&mut out);
    }
    out
}

impl Menu {
    pub fn create_slot(&mut self, index: i32, name: &[u8]) {
        let index = slot_check_index(index);
        // The new slot becomes the most recently saved one.
        let last = (0..SLOT_COUNT).filter(|&i| i != index).map(|i| self.slot_list[i].save_order).max().unwrap_or(0);
        let (opties, control) = (self.opties, self.control);
        let s = &mut self.slot_list[index];
        s.name = name32(name);
        s.save_order = last.wrapping_add(1);
        s.stage = 0;
        s.used = 1;
        s.money = 1000;
        s.opties = opties;
        s.control = control;
        clear_results(s);
        self.slots_already_loaded = true;
    }

    pub fn clear_slot(&mut self, index: i32) {
        let index = slot_check_index(index);
        let order = self.slot_list[index].save_order;
        for s in self.slot_list.iter_mut() {
            if s.save_order > order {
                s.save_order -= 1;
            }
        }
        let s = &mut self.slot_list[index];
        s.used = 0;
        s.save_order = 0;
        s.money = 0;
        clear_results(s);
    }

    pub fn reset_slot(&mut self, index: i32) {
        let (opties, control) = (self.opties, self.control);
        let s = &mut self.slot_list[slot_check_index(index)];
        s.save_order = 0;
        s.stage = 0;
        s.money = 1000;
        s.opties = opties;
        s.control = control;
        clear_results(s);
    }

    pub fn save_slotdbase(&self) {
        if !file_write(&slots_bytes(&self.slot_list), "SLOT.A2R") {
            eprintln!("save_slotdbase: cannot write {}", save_path("SLOT.A2R").display());
        }
    }

    pub fn init_slotdbase(&mut self) {
        let mut buf = vec![0u8; Slot::SIZE * SLOT_COUNT];
        if file_exist("SLOT.A2R") && file_read(&mut buf, "SLOT.A2R") {
            let mut r = Reader { p: &buf };
            for s in self.slot_list.iter_mut() {
                *s = Slot::read(&mut r);
            }
        } else {
            file_delete("SLOT.A2R");
            for s in self.slot_list.iter_mut() {
                *s = Slot { opties: self.opties, control: self.control, ..Slot::default() };
            }
            self.save_slotdbase();
        }
        self.slots_already_loaded = true;
    }

    pub fn get_last_saved_slot(&self) -> i32 {
        if !self.slots_already_loaded {
            return 0;
        }
        let mut best = 0;
        let mut order = 0;
        for (i, s) in self.slot_list.iter().enumerate() {
            if s.used != 0 && s.save_order > order {
                order = s.save_order;
                best = i as i32;
            }
        }
        best
    }

    pub fn set_active_slot(&mut self, index: i32) {
        let s = &self.slot_list[slot_check_index(index)];
        assert!(s.used != 0, "set_active_slot: slot {index} is not in use");
        self.opties = s.opties;
        self.control = s.control;
        self.active_slot = index;
    }

    pub fn update_active_slot_optie(&mut self) {
        let opties = self.opties;
        let s = &mut self.slot_list[slot_check_index(self.active_slot)];
        if s.used != 0 {
            s.opties = opties;
        }
    }

    pub fn update_active_slot_bediening(&mut self) {
        let control = self.control;
        let s = &mut self.slot_list[slot_check_index(self.active_slot)];
        if s.used != 0 {
            s.control = control;
        }
    }

    pub fn slot_naam_string(&self, index: i32) -> Vec<u8> {
        let s = &self.slot_list[slot_check_index(index)];
        if s.used != 0 { cstr(&s.name).to_vec() } else { b"L E E G".to_vec() } // "empty"
    }

    pub fn slot_level_string(&self, index: i32) -> Vec<u8> {
        let s = &self.slot_list[slot_check_index(index)];
        if s.used != 0 { LEVEL_NAAM[s.stage as usize].to_vec() } else { Vec::new() }
    }

    pub fn slot_count_total_tijd(&self, index: i32, stages: i32) -> i32 {
        let s = &self.slot_list[slot_check_index(index)];
        if s.used == 0 {
            return 0;
        }
        (0..stages).map(|i| s.time[slot_check_level(i)]).sum()
    }

    /// racer 0..4 for the rivals, 5 for the player.
    pub fn slot_count_total_tijd_speler(&self, index: i32, racer: usize) -> i32 {
        let s = &self.slot_list[slot_check_index(index)];
        if s.used == 0 {
            return 0;
        }
        (0..s.stage as i32)
            .map(|i| if racer == 5 { s.time[slot_check_level(i)] } else { s.time_racer[racer][slot_check_level(i)] })
            .sum()
    }

    /// "HHuMMmSSs": hours (uur), minutes and seconds of the player's total time.
    pub fn slot_tijd_string(&self, index: i32) -> Vec<u8> {
        let s = &self.slot_list[slot_check_index(index)];
        if s.used == 0 {
            return b"--u--m--s".to_vec();
        }
        let t = self.slot_count_total_tijd(index, s.stage as i32);
        format!("{:02}u{:02}m{:02}s", t / 3600, t / 60 % 60, t % 60).into_bytes()
    }
}
