//! Between the menu and the race (spel.cpp): reset_infio fills INFIO.INI's data for the
//! race; handle_game takes the result back into the player's slot.

use crate::game::Game;
use crate::gfx::screen::clear_screen;
use crate::gfx::wipes::{handle_wipe, WIPE_PIXELFADE};
use crate::menu::menu_types::cstr;
use crate::menu::mysound::{snd_fade_out, snd_play_looping};
use crate::menu::slot::{slot_check_index, slot_check_level};
use crate::menu::Menu;

impl Menu {
    /// Fills infio from the active player's slot and the options before a race.
    pub fn reset_infio(&self, g: &mut Game) {
        let s = &self.slot_list[slot_check_index(self.active_slot)];
        let o = &self.opties;
        let i = &mut g.infio;
        i.race_finished = 0;
        i.money = s.money;
        i.active_slot = self.active_slot as i16;
        i.sound = o.sound;
        i.resolution = o.resolution;
        i.view = o.view;
        i.control_type = self.control.control_type as i16;
        i.display_mode = o.display_mode;
        i.horizon = o.horizon;
        i.dashboard = o.dashboard;
        i.detail = o.detail;
        i.camera = o.camera;
        i.music_volume = o.music_volume;
        i.effects_volume = o.effects_volume;
        i.stage = s.stage;
        // Starting order: the result of the previous stage, or 1..6 on the first.
        if s.stage < 1 {
            i.position = 6;
            i.racer = [1, 2, 3, 4, 5];
        } else {
            let prev = slot_check_level(s.stage as i32 - 1);
            i.position = s.position[prev];
            for r in 0..5 {
                i.racer[r] = s.racer[r][prev];
            }
        }
        i.time = 0;
        i.time_racer = [0; 5];
        i.final_ = 0;
        i.ended_by = 0;
        for (k, key) in self.control.keys().iter().enumerate() {
            i.keys[k] = *key as i16;
        }
    }

    /// After a race: stores its result in the player's slot and shows the stage result,
    /// the classification and the scores. False when the series is over (the player
    /// then picks a slot again), true to go on to the main menu.
    pub fn handle_game(&mut self, g: &mut Game) -> bool {
        let mut snd = -1;
        self.active_slot = g.infio.active_slot as i32;
        if g.infio.race_finished == 0 {
            return true;
        }
        g.infio.race_finished = 0;
        let infio = g.infio;
        let idx = slot_check_index(self.active_slot);
        let stage = self.slot_list[idx].stage as i32;
        let level = slot_check_level(stage);
        {
            let s = &mut self.slot_list[idx];
            s.stage += 1;
            s.money = infio.money;
            s.time[level] = infio.time;
            s.position[level] = infio.position;
            for r in 0..5 {
                s.racer[r][level] = infio.racer[r];
                s.time_racer[r][level] = infio.time_racer[r];
            }
        }
        snd_play_looping(self.snd(), &mut snd, "rock.wav");
        self.menu_display_stand(g, stage);
        self.menu_display_classe(g);
        let s = self.slot_list[idx];
        self.add_new_levelscore(cstr(&s.name), s.time[level], stage, s.money);
        let at = self.get_last_entered_levelscore(stage);
        if (0..10).contains(&at) {
            self.menu_display_score(g, stage);
        }
        while snd_fade_out(self.snd(), &mut snd) {}

        if s.stage > 5 {
            // The last stage: the series goes into the high scores and the slot is freed.
            // (movie_play_outtro is empty in the original; the launcher plays a2win.smk.)
            clear_screen(2, 0);
            handle_wipe(2, WIPE_PIXELFADE, 600);
            clear_screen(1, 0);
            let total = self.slot_count_total_tijd(self.active_slot, s.stage as i32);
            self.add_new_hiscore(cstr(&s.name), total, s.stage as i32, s.money);
            let at = self.get_last_entered_hiscore();
            if (0..10).contains(&at) {
                self.menu_display_score(g, -1);
            }
            self.clear_slot(self.active_slot);
            self.active_slot = self.get_last_saved_slot();
            return false;
        }
        true
    }
}
