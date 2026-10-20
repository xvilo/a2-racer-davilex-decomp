//! The race (A2SPEL.DAT): main.cpp's WinMain, objio.cpp's race settings and states,
//! setracer.cpp's start positions, timers and ranking.

use crate::game::Game;
use crate::platform::sys::*;
use crate::race::camera::{CAMERA_CHASE, CAMERA_COCKPIT, CAMERA_OVERHEAD};
use crate::race::driver::{DRIVER_CIVILIAN, DRIVER_POLICE, DRIVER_RIVAL};
use crate::race::race_files::RaceFiles;
use crate::race::*;
use crate::xvga::inifile::read_ini_value_setting;
use crate::xvga::sound::xvga_set_sound_files;

/// The original ran a frame per screen refresh as fast as the machine allowed; the
/// port runs its frames at a fixed rate.
const RACE_FPS: u32 = 30;

const FINE_AMOUNTS: [i32; 8] = [250, 500, 800, 1200, 1500, 2000, 2000, 2000];

/// Wall-clock seconds, as the original's time().
pub fn race_time_now() -> i32 {
    std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).map(|d| d.as_secs() as i32).unwrap_or(0)
}

impl Race {
    // --- timers (main.cpp, setracer.cpp) -------------------------------------------------

    fn start_countdown(&mut self, seconds: i32) {
        self.countdown_seconds = seconds;
        self.countdown_last_tick = race_time_now();
    }

    fn countdown_expired(&mut self) -> bool {
        if self.countdown_seconds != 0 {
            let now = race_time_now();
            self.countdown_seconds -= now - self.countdown_last_tick;
            self.countdown_last_tick = now;
            if self.countdown_seconds < 1 {
                self.countdown_seconds = 0;
            }
        }
        self.countdown_seconds == 0
    }

    fn init_race_timers(&mut self, limit: i32) {
        self.time_left = limit;
        self.time_clock = race_time_now();
        for t in self.racer_times.iter_mut() {
            *t = RacerTime { start: race_time_now(), finish_time: 0, place: 1 };
        }
    }

    fn restart_time_clock(&mut self) {
        self.time_clock = race_time_now();
    }

    fn update_time_left(&mut self) {
        if self.time_left == 0 {
            return;
        }
        let now = race_time_now();
        self.time_left -= now - self.time_clock;
        self.time_clock = now;
        if self.time_left < 1 {
            self.infio.ended_by = 3;
            self.race_finished_ok = false;
            self.set_race_state(RACE_OUT_OF_TIME);
            self.time_left = 0;
            self.play_sound_effect(0x26, false);
        }
    }

    /// Fuel per chunk driven; running out ends the race (objio.cpp).
    pub fn consume_fuel(&mut self, amount: i32) {
        self.fuel -= amount;
        if self.fuel < 1 {
            self.infio.ended_by = 1;
            self.race_finished_ok = false;
            self.set_race_state(RACE_OUT_OF_FUEL);
            self.play_sound_effect(0x27, false);
            self.fuel = 0;
        }
    }

    /// Money for fines; running out ends the race.
    pub fn pay_money(&mut self, amount: i32) {
        self.money -= amount;
        if self.money < 0 {
            self.infio.ended_by = 2;
            self.race_finished_ok = false;
            self.set_race_state(RACE_NO_MONEY);
            self.play_sound_effect(0x25, false);
            self.money = 0;
        }
    }

    fn next_fine(&mut self) {
        self.current_fine = FINE_AMOUNTS[self.fine_count];
        self.fine_count = (self.fine_count + 1).min(5);
    }

    // --- ranking (setracer.cpp) ----------------------------------------------------------

    /// racer_progress_in_section: how far along its chunk a racer is.
    fn racer_progress_in_section(&self, racer: usize) -> f32 {
        let o = &self.opp.data[racer];
        let s = &self.weg.sections[o.chunk as usize];
        let p = self.pools.vertices3d[s.point];
        (o.x - p.x) * math3d::cos_a(s.heading) + (o.z - p.z) * math3d::sin_a(s.heading)
    }

    /// For each of the six racers (player first) its place, 1..6.
    pub fn compute_race_ranking(&self) -> Option<[i32; 6]> {
        if self.opp.count() < 6 || self.weg.sections.is_empty() {
            return None;
        }
        let key: Vec<i32> = (0..6).map(|i| self.opp.data[i].chunk * 100 + self.racer_progress_in_section(i) as i32).collect();
        let mut sorted = key.clone();
        sorted.sort(); // bubble_sort_ints
        let mut ranking = [0; 6];
        for i in 0..6 {
            let k = sorted.iter().position(|&s| s == key[i]).unwrap() as i32;
            ranking[i] = if self.track().direction == 0 { k + 1 } else { 6 - k };
        }
        Some(ranking)
    }

    /// A racer's place now (1..6).
    pub fn get_racer_entry(&self, racer: usize) -> i32 {
        self.compute_race_ranking().map(|r| r[racer]).unwrap_or(racer as i32 + 1)
    }

    pub fn set_racer_finish_time(&mut self, racer: usize) {
        if self.racer_times[racer].finish_time == 0 {
            self.racer_times[racer].finish_time = race_time_now() - self.racer_times[racer].start;
            self.racer_times[racer].place = self.get_racer_entry(racer);
        }
    }

    /// A racer still on the road gets the player's time and up to a minute more.
    fn get_racer_finish_time(&mut self, racer: usize) -> i32 {
        if self.racer_times[racer].finish_time != 0 {
            return self.racer_times[racer].finish_time;
        }
        self.racer_times[0].finish_time + self.rand() % 0x3c + 5
    }

    fn get_racer_place(&self, racer: usize) -> i32 {
        if self.racer_times[racer].finish_time != 0 {
            return self.racer_times[racer].place;
        }
        self.get_racer_entry(racer)
    }

    // --- start (setracer.cpp) ------------------------------------------------------------

    /// The grid: two cars a row behind the start, alternately in the lanes left and right.
    fn set_racer_start_position(&mut self, racer: usize, position: i32, start_chunk: i32) {
        let direction = self.track().direction;
        let (mut lane, sign) = if direction == 0 { (8, -1) } else { (2, 1) };
        let offset = if direction == 0 { -20.0 } else { 20.0 };
        let side = if position % 2 == 1 { 1.0 } else { -1.0 };
        let chunk = start_chunk - ((position - 1) / 2 + 1) * sign;
        lane += (position % 2) * sign;
        if racer == 0 {
            self.load_player_object(direction, chunk, lane, side * offset);
        } else {
            self.place_opponent(racer, direction, chunk, lane, side * offset);
            let d = self.opp.data[racer].field_e0 as usize;
            self.drivers.driver_list[d].speed = self.opp.racer_speed as f32;
        }
    }

    /// The racers 1..5, police and traffic (create_all_opponents): opponent numbers 1..5
    /// are the rivals, then the police, then the civilians in turn of car type.
    fn create_all_opponents(&mut self) {
        for ty in [0x15, 0x12, 0x11, 0x16, 0x14] {
            self.create_opponent(ty, DRIVER_RIVAL);
        }
        for _ in 0..self.police_count {
            self.create_opponent(0xc, DRIVER_POLICE);
        }
        let mut ty = 0;
        for _ in 0..self.civilian_count {
            self.create_opponent(ty, DRIVER_CIVILIAN);
            loop {
                ty += 1;
                if ty == 0x10 {
                    ty = 0;
                }
                if ty != 4 && ty != 0xc {
                    break;
                }
            }
        }
    }

    fn place_rival_racers(&mut self, start_chunk: i32) {
        for r in 0..5 {
            let position = self.infio.racer[r] as i32;
            self.set_racer_start_position(r + 1, position, start_chunk);
        }
    }

    /// A police car parked on the hard shoulder (the two outer lanes).
    fn set_police_car(&mut self, police: usize, chunk: i32) {
        let direction = self.track().direction;
        let lane = if direction == 0 { 10 - self.rand() % 2 } else { self.rand() % 2 };
        self.place_opponent(police + 6, direction, chunk, lane, 0.0);
    }

    /// Spread along the road, one per stretch of (chunks / police / 2) or so.
    fn place_police_cars(&mut self) {
        let stretch = self.weg.chunk_count / (self.police_count as i32 * 2);
        let mut chunk = 0;
        for police in 0..self.police_count {
            chunk += self.rand() % stretch + stretch;
            self.set_police_car(police, chunk);
        }
    }

    fn player_at_finish(&self) -> bool {
        if self.track().direction == 0 {
            self.player_chunk == 0xf
        } else {
            self.weg.chunk_count - 0xf == self.player_chunk
        }
    }

    fn set_all_objects(&mut self) {
        let position = self.infio.position as i32;
        let n = self.weg.chunk_count;
        let start = if self.track().direction == 0 { n - 0xf } else { 0xf };
        self.set_racer_start_position(0, position, start);
        self.create_all_opponents();
        self.place_rival_racers(start);
        self.place_police_cars();
        self.scatter_traffic(self.police_count + 6, 0x1e);
        self.place_random_pionnen();
        if self.track().direction == 0 {
            self.place_static_object_on_road(0x17, 0xf, 1, 1, 0x3c);
            self.place_static_object_on_road(0x16, n - 0x10, 1, 1, 0x3c);
        } else {
            self.place_static_object_on_road(0x16, 0x10, 2, 1, 0x3c);
            self.place_static_object_on_road(0x17, n - 0xf, 2, 1, 0x3c);
        }
        for ty in 0x26..=0x2b {
            self.load_static_object(ty, 0.0, 0.0, 0);
        }
    }

    // --- settings and states (objio.cpp) -------------------------------------------------

    fn race_positions_valid(&self) -> bool {
        let i = &self.infio;
        let p = [i.position, i.racer[0], i.racer[1], i.racer[2], i.racer[3], i.racer[4]];
        p.iter().all(|&x| (1..=6).contains(&x)) && (0..6).all(|a| (0..6).all(|b| a == b || p[a] != p[b]))
    }

    fn init_race_settings(&mut self) {
        self.infio.init_infio();
        if self.files.race_disc_ini("A2.INI") {
            self.bonus.time_bonus = read_ini_value_setting("BONUS", "TIME") as i32 * 0x3c;
            // TRACK/START1..3: the time limits of stages 1..3, and 4..6 driving them back.
            for (k, key) in ["START1", "START2", "START3"].iter().enumerate() {
                let t = read_ini_value_setting("TRACK", key) as i32 * 0x3c;
                self.track_info[k].time_limit = t;
                self.track_info[5 - k].time_limit = t;
            }
        }
        if !self.race_positions_valid() {
            self.infio.racer = [1, 2, 3, 4, 5];
            self.infio.position = 6;
        }
        let i = self.infio;
        self.fuel = 300;
        self.race_running = false;
        self.controls_enabled = false;
        self.detail_level = i.detail as i32;
        self.race_finished_ok = false;
        self.track_index = if (0..=5).contains(&i.stage) { i.stage as usize } else { 0 };
        self.init_race_timers(self.track().time_limit);
        self.money = if i.money < 1 || 9999 < i.money { 1000 } else { i.money };
        self.dashboard_on_screen = i.dashboard == 1;
        // Display modes: 0 3D card (bilinear textures), 1 320x200, 2 640x480. The port
        // always draws at the window's size; 320x200's smaller HUD is kept.
        self.texture_filtering = i.display_mode == 0;
        self.screen_resolution = (i.display_mode == 1) as i32;
        if i.display_mode == 1 {
            self.dashboard_on_screen = false;
        }
        self.horizon_on = i.horizon == 1;
        let s = &mut self.sndfx;
        s.volume_offset = (5 - i.effects_volume as i32) * 300;
        s.volume_attenuation = (4 - i.effects_volume as i32) * 500;
        s.music_attenuation = (4 - i.music_volume as i32) * 500;
        // OPTION/GELUID: 0 all, 1 effects only, 2 none, 3 music only.
        (s.music_enabled, s.engine_sound_enabled) = match i.sound {
            0 => (true, true),
            1 => (false, true),
            2 => (false, false),
            3 => (true, false),
            _ => (s.music_enabled, s.engine_sound_enabled),
        };
    }

    pub fn set_race_state(&mut self, state: i32) -> i32 {
        if (self.race_state == RACE_RUNNING || state == RACE_RUNNING || state == RACE_EXIT) && self.race_state != RACE_EXIT {
            self.race_state = state;
        }
        match self.race_state {
            RACE_RUNNING => {
                self.race_running = true;
                self.controls_enabled = true;
                0
            }
            RACE_FINE => {
                self.next_fine();
                self.start_countdown(7);
                self.play_sound_effect(0x23, false);
                0x23
            }
            RACE_OUT_OF_TIME | RACE_OUT_OF_FUEL => {
                self.halt_player_car();
                self.start_countdown(7);
                self.controls_enabled = false;
                0
            }
            RACE_NO_MONEY => {
                self.start_countdown(0xc);
                self.controls_enabled = false;
                0
            }
            RACE_PAUSED => {
                self.race_running = false;
                self.controls_enabled = false;
                9
            }
            s => s,
        }
    }

    /// update_race: the states' timing and the pictures they show.
    fn update_race(&mut self) {
        let dash = self.dashboard_on_screen;
        match self.race_state {
            RACE_INTRO => {
                if !self.intro_started {
                    self.start_camera_intro();
                    self.intro_started = true;
                }
                self.camera.camera_height -= 10.0;
                if self.camera.camera_height <= 0.0 {
                    self.set_camera_position(CAMERA_COCKPIT);
                    self.race_state = RACE_COUNTDOWN;
                }
            }
            RACE_COUNTDOWN => {
                if !self.countdown_started {
                    self.countdown_step = 0;
                    self.start_countdown(1);
                    self.countdown_started = true;
                }
                if self.countdown_expired() {
                    self.countdown_step += 1;
                    if self.countdown_step == 5 {
                        self.race_running = true;
                        self.controls_enabled = true;
                        self.race_state = RACE_RUNNING;
                        self.countdown_step = 4;
                    } else {
                        self.play_sound_effect(if self.countdown_step < 4 { 0x1a } else { 0x1b }, false);
                        self.start_countdown(1);
                    }
                }
                self.hud_image(0x12, self.countdown_step, 0x96, if dash { 0xa0 } else { 200 }, 2);
            }
            RACE_FINISHED => {
                if !self.finish_started {
                    self.stop_player_car();
                    self.start_countdown(10);
                    self.finish_started = true;
                }
                if self.countdown_expired() || keyispressed(0x20) || keyispressed(0x0d) {
                    self.race_state = RACE_EXIT;
                }
                // The waving flag.
                self.hud_image(0x13, self.finish_frame, 0x140, if dash { 0xa0 } else { 200 }, 0);
                self.finish_frame = (self.finish_frame + 1) % 0x10;
            }
            RACE_FINE => {
                let (x, y) = (0x186, if dash { 0xbe } else { 0xe6 });
                let f = self.current_fine;
                self.hud_image(0x16, 0, 0xa0, if dash { 0x3c } else { 100 }, 2);
                if f / 1000 != 0 {
                    self.hud_image(0x1c, f / 1000, x, y, 0);
                }
                self.hud_image(0x1c, f % 1000 / 100, x + 0xc, y, 0);
                self.hud_image(0x1c, f % 100 / 10, x + 0xc * 2, y, 0);
                self.hud_image(0x1c, f % 10, x + 0xc * 3, y, 0);
                if self.countdown_expired() || keyispressed(0x0d) || keyispressed(0x20) {
                    self.set_race_state(RACE_RUNNING);
                    self.pay_money(f);
                }
            }
            RACE_OUT_OF_TIME | RACE_OUT_OF_FUEL => {
                let anim = if self.race_state == RACE_OUT_OF_TIME { 0x20 } else { 0x22 };
                self.hud_image(anim, 0, 0x140, if dash { 0x8c } else { 0xf0 }, 0);
                if self.countdown_expired() {
                    self.set_race_state(RACE_EXIT);
                }
            }
            RACE_NO_MONEY => {
                self.hud_image(0x18, 0, 0xa0, if dash { 0x3c } else { 100 }, 2);
                if self.countdown_expired() || keyispressed(0x0d) || keyispressed(0x20) {
                    self.set_race_state(RACE_EXIT);
                }
            }
            RACE_PAUSED => {
                self.hud_image(0x1a, 0, 0xa0, if dash { 100 } else { 0x8c }, 2);
                if keyispressed(0x50) || keyispressed(0x0d) || keyispressed(0x20) {
                    let mut input = SysInput::default();
                    while keyispressed(0x50) {
                        sys_poll(&mut input);
                        sys_sleep(5);
                    }
                    self.restart_time_clock();
                    self.set_race_state(RACE_RUNNING);
                }
            }
            _ => {}
        }
    }

    // --- WinMain ---------------------------------------------------------------------

    /// WinMain's A2.INI settings. It reads POLICE<n> and CIVIL<n> before the stage is
    /// known, so stage 1's numbers count for every stage; kept.
    fn read_tweaks(&mut self) {
        self.police_count = 8;
        self.civilian_count = 0x96;
        if !self.files.race_disc_ini("A2.INI") {
            return;
        }
        let d = &mut self.drivers;
        d.racer_range = read_ini_value_setting("TWEAK", "RACERRANGE") as i32;
        d.racer_high_speed = read_ini_value_setting("TWEAK", "RACERHIGHSPEED") as f32;
        d.racer_low_speed = read_ini_value_setting("TWEAK", "RACERLOWSPEED") as f32;
        self.police_count = read_ini_value_setting("TWEAK", "POLICE0") as usize;
        self.civilian_count = read_ini_value_setting("TWEAK", "CIVIL0") as usize;
        self.opp.police_max_speed = read_ini_value_setting("RACE", "POLICESPEED") as i32;
        self.opp.racer_speed = read_ini_value_setting("RACE", "RACERSPEED") as i32;
        if self.police_count == 0 {
            self.police_count = 8;
        }
        if self.civilian_count == 0 {
            self.civilian_count = 0x96;
        }
    }

    fn run(&mut self, g: &Game) -> bool {
        self.read_tweaks();
        self.init_race_settings();
        self.render.textures.alloc_textures(0x50);
        self.render.change_viewport_size(0, 0, 640, if self.dashboard_on_screen { 300 } else { 480 });
        let back = self.render.textures.load_texture(&self.files.rcs, "back.pcx");
        self.render.set_background(back);
        self.load_samples();
        if !self.init_hud(&g.data_dir) {
            return false;
        }
        self.render.textures.set_texture_filter(self.texture_filtering);
        // init_anims_and_standard_textures
        let objects = 0x9c + self.police_count + self.civilian_count + 400;
        self.pools.alloc_objects3d(objects);
        self.pools.alloc_3d_arrays(objects, objects, 200000, 100000);
        self.load_car_textures();
        self.load_static_object_textures();
        self.weg.road_texture = self.render.textures.load_texture(&self.files.rcs, "2baans.pcx");
        self.weg.grass_texture = self.render.textures.load_texture(&self.files.rcs, "grass2.pcx");
        self.init_opponents(self.police_count + 0x38 + self.civilian_count);
        if !self.load_weg_data(self.track().track, 0x14) {
            return false;
        }
        self.init_opponent_map();
        self.init_drivers(self.opp.count(), 5, self.police_count);
        self.init_active_opponent_list();
        self.camera.init_cameras();
        self.init_controls();
        if self.horizon_on {
            self.load_static_object(0, 0.0, 0.0, 0);
        }
        self.init_pionnen();
        self.set_all_objects();
        if self.detail_level == 1 {
            self.init_static_objects(self.track_index);
        }
        self.start_music(self.track().music);
        self.place_all_bonus_items();
        self.race_state = RACE_INTRO;
        self.start_camera_intro();

        let mut next = sys_ticks();
        let mut input = SysInput::default();
        while self.race_state != RACE_EXIT {
            sys_poll(&mut input);
            if input.quit {
                self.infio.ended_by = 0;
                self.race_finished_ok = false;
                break;
            }
            self.update_racers();
            sys_gl_begin_frame();
            self.render_3d_frame();
            self.hud_begin();
            self.draw_radar();
            // draw_time_left keeps the clock still until the race runs.
            if !self.race_running {
                self.restart_time_clock();
            }
            self.update_time_left();
            self.draw_dashboard();
            self.draw_bonus_status();
            self.update_pionnen();
            self.update_race();
            self.hud_end();
            sys_gl_swap();
            self.mark_active_racers();
            if keyispressed(0x50) && self.race_state == RACE_RUNNING {
                while keyispressed(0x50) {
                    sys_poll(&mut input);
                    sys_sleep(5);
                }
                self.set_race_state(RACE_PAUSED);
            }
            if self.player_at_finish() && self.race_state != RACE_EXIT && self.race_state != RACE_FINISHED {
                self.set_racer_finish_time(0);
                self.play_sound_effect(0x2b, false);
                self.race_state = RACE_FINISHED;
                self.race_finished_ok = true;
            }
            if keyispressed(0x70) {
                self.set_camera_position(CAMERA_COCKPIT);
            }
            if keyispressed(0x71) {
                self.set_camera_position(CAMERA_CHASE);
            }
            if keyispressed(0x74) {
                self.hud.radar_on = !self.hud.radar_on;
            }
            if keyispressed(0x1b) && self.race_state == RACE_RUNNING {
                self.infio.ended_by = 0;
                self.race_finished_ok = false;
                self.race_state = RACE_EXIT;
            }
            if keyispressed(0x5a) && keyispressed(0x4f) && keyispressed(0x11) && keyispressed(0x7b) {
                self.set_camera_position(CAMERA_OVERHEAD);
            }
            // A fixed frame rate.
            next = next.wrapping_add(1000 / RACE_FPS);
            let wait = next.wrapping_sub(sys_ticks()) as i32;
            if wait > 0 {
                sys_sleep(wait as u32);
            } else if wait < -200 {
                next = sys_ticks();
            }
        }

        self.exit_samples();

        // The result for the menu.
        if !self.race_finished_ok {
            self.infio.time = 0;
        } else {
            self.infio.time = self.get_racer_finish_time(0);
            self.infio.money = self.money;
            self.infio.position = self.get_racer_place(0) as i16;
            for r in 0..5 {
                self.infio.racer[r] = self.get_racer_place(r + 1) as i16;
                self.infio.time_racer[r] = self.get_racer_finish_time(r + 1);
            }
        }
        self.infio.race_finished = self.race_finished_ok as u8;
        self.infio.save_infio();
        true
    }
}

/// The race, run between two menu runs as A2.EXE ran A2SPEL.DAT. Reads INFIO.INI,
/// writes the result back to it (and into g.infio).
pub fn race_main(g: &mut Game) -> i32 {
    if !sys_gl() {
        eprintln!("the race needs OpenGL");
        return 1;
    }
    let Some(files) = RaceFiles::race_files_open(&g.data_dir) else { return 1 };
    // Each race starts from scratch, as a fresh A2SPEL.DAT process did.
    let mut race = Race::new(files);
    let ok = race.run(g);
    race.exit_hud();
    xvga_set_sound_files(Some(g.menu_files.clone()), 0x28);
    race.render.textures.free_textures();
    g.infio = race.infio;
    if ok { 0 } else { 1 }
}
