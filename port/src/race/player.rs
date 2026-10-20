//! The player's car: its controls, gearbox and steering (mycontro.cpp, opptools.cpp),
//! and its object (myinit.cpp). Also the car type table every car comes from.

use crate::race::math3d::{cos_a, sin_a, Matrix};
use crate::race::object3d::Update;
use crate::race::opponent::LANE_OFFSETS;
use crate::race::Race;
use crate::xvga::joystick::{calibrate_joysticks, get_attached_joysticks, get_joystick_position, handle_joystick,
    keyispressed_1, set_joystick_keys};

pub struct CarType {
    pub mesh: &'static str,
    #[allow(dead_code)]
    pub texture_hi: &'static str,
    pub texture: &'static str,
    pub max_speed: f32,
    pub car_class: i32,
    pub mass: f32,
}

pub const CAR_TYPE_COUNT: usize = 23;
/// The red BMW.
pub const PLAYER_CAR_TYPE: usize = 19;

/// car_types in A2SPEL.DAT: model, (unused) high-detail texture, texture, top speed,
/// class and mass.
pub const CAR_TYPES: [CarType; CAR_TYPE_COUNT] = [
    CarType { mesh: "porche.d3d", texture_hi: "porche.pcx", texture: "k_porche.pcx", max_speed: 9.0, car_class: 0, mass: 900.0 },
    CarType { mesh: "interl.d3d", texture_hi: "il.pcx", texture: "k_il.pcx", max_speed: 7.0, car_class: 0, mass: 5000.0 },
    CarType { mesh: "camaro.d3d", texture_hi: "camaro.pcx", texture: "k_camaro.pcx", max_speed: 9.0, car_class: 0, mass: 1500.0 },
    CarType { mesh: "golf.d3d", texture_hi: "golf.pcx", texture: "k_golf.pcx", max_speed: 8.5, car_class: 0, mass: 1000.0 },
    CarType { mesh: "bmw.d3d", texture_hi: "bmw.pcx", texture: "k_bmw.pcx", max_speed: 10.0, car_class: 0, mass: 1300.0 },
    CarType { mesh: "van.d3d", texture_hi: "vanmap.pcx", texture: "k_vanmap.pcx", max_speed: 9.0, car_class: 0, mass: 1000.0 },
    CarType { mesh: "lotus.d3d", texture_hi: "lotus.pcx", texture: "k_lotus.pcx", max_speed: 11.0, car_class: 0, mass: 1500.0 },
    CarType { mesh: "viper.d3d", texture_hi: "viper.pcx", texture: "k_viper.pcx", max_speed: 10.0, car_class: 0, mass: 1500.0 },
    CarType { mesh: "panda.d3d", texture_hi: "panda.pcx", texture: "k_panda.pcx", max_speed: 6.0, car_class: 0, mass: 800.0 },
    CarType { mesh: "merceds.d3d", texture_hi: "mercedes.pcx", texture: "k_merced.pcx", max_speed: 8.0, car_class: 0, mass: 900.0 },
    CarType { mesh: "merceds.d3d", texture_hi: "merceds2.pcx", texture: "k_merce2.pcx", max_speed: 8.5, car_class: 0, mass: 900.0 },
    CarType { mesh: "vracht.d3d", texture_hi: "vracht.pcx", texture: "k_vracht.pcx", max_speed: 8.0, car_class: 0, mass: 10000.0 },
    CarType { mesh: "police.d3d", texture_hi: "police.pcx", texture: "k_police.pcx", max_speed: 18.0, car_class: 0, mass: 900.0 },
    CarType { mesh: "cx.d3d", texture_hi: "cx.pcx", texture: "k_cx.pcx", max_speed: 10.0, car_class: 0, mass: 900.0 },
    CarType { mesh: "jaguar.d3d", texture_hi: "jaguar.pcx", texture: "k_jaguar.pcx", max_speed: 10.0, car_class: 0, mass: 900.0 },
    CarType { mesh: "volvo.d3d", texture_hi: "volvo.pcx", texture: "k_volvo.pcx", max_speed: 10.0, car_class: 0, mass: 900.0 },
    CarType { mesh: "werkweg2.d3d", texture_hi: "werkweg2.pcx", texture: "werkweg2.pcx", max_speed: 0.0, car_class: 0, mass: 10000.0 },
    CarType { mesh: "porche.d3d", texture_hi: "r_porche.pcx", texture: "r_porche.pcx", max_speed: 9.0, car_class: 0, mass: 900.0 },
    CarType { mesh: "camaro.d3d", texture_hi: "r_camaro.pcx", texture: "r_camaro.pcx", max_speed: 9.0, car_class: 0, mass: 1500.0 },
    CarType { mesh: "bmw.d3d", texture_hi: "r_bmw.pcx", texture: "r_bmw.pcx", max_speed: 10.0, car_class: 0, mass: 1300.0 },
    CarType { mesh: "jaguar.d3d", texture_hi: "r_jaguar.pcx", texture: "r_jaguar.pcx", max_speed: 10.0, car_class: 0, mass: 900.0 },
    CarType { mesh: "lotus.d3d", texture_hi: "r_lotus.pcx", texture: "r_lotus.pcx", max_speed: 11.0, car_class: 0, mass: 1500.0 },
    CarType { mesh: "viper.d3d", texture_hi: "r_viper.pcx", texture: "r_viper.pcx", max_speed: 10.0, car_class: 0, mass: 1500.0 },
];

/// Heading change per frame by steering position (0 full left .. 28 full right), and
/// the divisor by speed: faster cars turn less.
const STEER_TABLE: [f32; 29] = [
    14.0, 13.0, 12.0, 11.0, 10.0, 9.0, 8.0, 7.0, 6.0, 5.0, 4.0, 3.0, 2.0, 1.0, 0.0, -1.0, -2.0, -3.0, -4.0, -5.0, -6.0,
    -7.0, -8.0, -9.0, -10.0, -11.0, -12.0, -13.0, -14.0,
];
const STEER_SPEED_DIVISOR: [f32; 16] = [1.01, 1.1, 1.2, 1.3, 1.3, 1.3, 1.3, 1.3, 1.4, 1.5, 1.5, 1.6, 1.8, 2.0, 2.0, 2.0];

/// The gearbox: steps of (rpm, speed, gear), filled by add_rpm_steps.
#[derive(Clone, Copy, Default)]
pub struct RpmStep {
    pub rpm: i32,
    pub speed: f32,
    pub gear: i32,
}

/// A port-only test aid (A2_AUTOPILOT=1): keeps the car in its lane, below
/// police-alerting speed, so scripts can drive a whole stage.
const RACE_AUTOPILOT_PATIENCE: i32 = 30;

pub struct Player {
    /// Per car type: its texture and (once loaded) model frame.
    pub car_texture_id: [i32; CAR_TYPE_COUNT],
    pub car_mesh_id: [i32; CAR_TYPE_COUNT],
    rpm_table: Vec<RpmStep>,
    rpm_step: usize,
    idle_rpm_step: usize,
    current_gear: i32,
    pub engine_rpm: i32,
    key_left: i32,
    key_right: i32,
    key_gas: i32,
    key_brake: i32,
    /// 1 keyboard, 2 joypad, 3 steering wheel.
    control_type: i32,
    /// The port's: joystick 1 is there.
    joystick_attached: bool,
    /// 0..28, 14 straight.
    steering_position: i32,
    steering_return_rate: f32,
    /// DAT_00457750: the position the car steers with.
    steer_index: i32,
    /// 0 none yet, 1 forward, 2 reverse.
    accelerator_input: i32,
    wrong_way_warned: bool,
    engine_started: bool,
    autopilot: Option<(f32, bool)>,
    autopilot_lane: f32,
    autopilot_blocked: i32,
}

impl Default for Player {
    fn default() -> Player {
        let mode = std::env::var("A2_AUTOPILOT").ok();
        Player {
            car_texture_id: [-1; CAR_TYPE_COUNT],
            car_mesh_id: [-1; CAR_TYPE_COUNT],
            rpm_table: Vec::new(),
            rpm_step: 0,
            idle_rpm_step: 0,
            current_gear: 0,
            engine_rpm: 0,
            key_left: 0,
            key_right: 0,
            key_gas: 0,
            key_brake: 0,
            control_type: 1,
            joystick_attached: false,
            steering_position: 14,
            steering_return_rate: 1.0,
            steer_index: 14,
            accelerator_input: 0,
            wrong_way_warned: false,
            engine_started: false,
            // "fast": full throttle, police or not; "nofuel": the tank runs dry.
            autopilot: mode.map(|m| (if m == "fast" { 16.0 } else { 9.0 }, m != "nofuel")),
            autopilot_lane: 0.0,
            autopilot_blocked: 0,
        }
    }
}

impl Player {
    /// add_rpm_steps: `count` steps for a gear, speed and rpm rising evenly.
    fn add_rpm_steps(&mut self, gear: i32, speed_from: f32, speed_to: f32, rpm_from: f32, rpm_to: f32, count: f32) {
        let (dspeed, drpm) = ((speed_to - speed_from) / count, (rpm_to - rpm_from) / count);
        let (mut speed, mut rpm) = (speed_from, rpm_from);
        let mut k = 0.0f32;
        while k < count {
            self.rpm_table.push(RpmStep { rpm: rpm as i32, speed, gear });
            rpm += drpm;
            speed += dspeed;
            k += 1.0;
        }
    }

    pub fn select_rpm_step_for_speed(&mut self, speed: f32) {
        self.rpm_step = self.rpm_table.iter().rposition(|s| s.speed <= speed).unwrap_or(0);
    }

    /// update_steering: 1 left, 2 right, 3 neither (back to straight).
    fn update_steering(&mut self, input: i32, look_offset: i32) {
        match input {
            1 => {
                if self.steering_position > 0 {
                    self.steering_position = ((self.steering_position as f32) - self.steering_return_rate) as i32;
                    self.steering_position = self.steering_position.max(0);
                }
            }
            2 => {
                if self.steering_position < 28 {
                    self.steering_position = ((self.steering_position as f32) + self.steering_return_rate) as i32;
                    if self.steering_position > 28 {
                        self.steering_position = 27;
                    }
                }
            }
            _ => {}
        }
        if input == 1 || input == 2 {
            self.steering_return_rate = (self.steering_return_rate + 0.5).min(6.0);
        } else if input == 3 {
            if self.steering_position > 14 {
                self.steering_position = (((self.steering_position as f32) - self.steering_return_rate) as i32).max(14);
            }
            if self.steering_position < 14 {
                self.steering_position = (((self.steering_position as f32) + self.steering_return_rate) as i32).min(14);
            }
            self.steering_return_rate = 1.0;
        }
        if look_offset == 0 {
            self.steer_index = self.steering_position;
        }
    }

    /// 0..28, 14 straight (for the dashboard's steering wheel).
    pub fn player_steering_position(&self) -> i32 {
        self.steering_position
    }
}

impl Race {
    pub fn load_car_textures(&mut self) {
        // Each race was a fresh process in the original; model frames are loaded again.
        for (i, ct) in CAR_TYPES.iter().enumerate() {
            self.player.car_texture_id[i] = self.render.textures.load_texture(&self.files.rcs, ct.texture);
            self.player.car_mesh_id[i] = -1;
        }
    }

    /// Keys and the gearbox table from INFIO.INI's settings.
    pub fn init_controls(&mut self) {
        let autopilot = self.player.autopilot;
        let (ids, meshes) = (self.player.car_texture_id, self.player.car_mesh_id);
        let p = &mut self.player;
        *p = Player { autopilot, car_texture_id: ids, car_mesh_id: meshes, ..Player::default() };
        let i = &self.infio;
        p.key_left = i.key_left() as i32;
        p.key_right = i.key_right() as i32;
        p.key_gas = i.key_gas() as i32;
        p.key_brake = i.key_brake() as i32;
        p.control_type = match i.control_type {
            1 => 2,
            2 => 3,
            _ => 1,
        };
        // calibrate_joysticks ran at the race's start in the original.
        calibrate_joysticks();
        p.joystick_attached = get_attached_joysticks() & 1 != 0;
        set_joystick_keys(1, 0, p.key_left);
        set_joystick_keys(1, 1, p.key_right);
        set_joystick_keys(1, 2, p.key_gas);
        set_joystick_keys(1, 3, p.key_brake);
        p.add_rpm_steps(0, -2.5, 0.0, 50.0, 0.0, 7.0); // reverse
        p.idle_rpm_step = p.rpm_table.len();
        p.rpm_step = p.idle_rpm_step;
        p.add_rpm_steps(1, 0.0, 2.5, 0.0, 55.0, 7.0);
        p.add_rpm_steps(2, 2.5, 4.0, 35.0, 60.0, 12.0);
        p.add_rpm_steps(3, 4.0, 6.0, 40.0, 63.0, 15.0);
        p.add_rpm_steps(4, 6.0, 8.0, 45.0, 65.0, 30.0);
        p.add_rpm_steps(5, 8.0, 15.0, 45.0, 60.0, 80.0);
        self.sndfx.init_engine_sound_tables();
    }

    /// update_accelerator: 1 more throttle (in the current direction), 2 brake, 3 neither.
    fn update_accelerator(&mut self, input: i32) {
        let p = &mut self.player;
        let (mut boost_freq, mut boost_vol) = (0, 0);
        if input == 1 && p.accelerator_input != 0 {
            if p.accelerator_input == 1 {
                if p.rpm_step < p.rpm_table.len() - 1 {
                    p.rpm_step += 1;
                }
            } else if p.rpm_step > 0 {
                p.rpm_step -= 1;
            }
            boost_freq = 6000;
            boost_vol = 300;
        } else if (input == 2 || input == 3) && p.accelerator_input != 0 {
            let step = if input == 2 { 4 } else { 1 }; // braking or rolling out
            if p.accelerator_input == 1 {
                if p.rpm_step > p.idle_rpm_step {
                    p.rpm_step = p.rpm_step.saturating_sub(step).max(p.idle_rpm_step);
                }
            } else if p.rpm_step < p.idle_rpm_step {
                p.rpm_step = (p.rpm_step + step).min(p.idle_rpm_step);
            }
        }
        let s = p.rpm_table[p.rpm_step];
        let old_gear = p.current_gear;
        p.engine_rpm = s.rpm;
        p.current_gear = s.gear;
        self.player_speed = s.speed;
        if old_gear != s.gear {
            self.play_gear_change_sound(s.gear, old_gear);
        }
        if self.race_running {
            self.set_engine_sound(s.rpm, boost_freq, boost_vol);
        }
    }

    /// The autopilot's steering: -1 left, 1 right, 0 straight.
    fn autopilot_steer(&mut self) -> i32 {
        let Some((_, refuel)) = self.player.autopilot else { return 0 };
        let dir = self.track().direction;
        let w = &self.weg;
        let ch = &w.chunks[self.player_chunk as usize];
        let c = self.pools.vertices3d[w.sections[ch.section].point];
        let lateral = (self.player_x - c.x) * ch.side[0] + (self.player_z - c.z) * ch.side[2];
        let p = &mut self.player;
        if p.autopilot_lane == 0.0 {
            p.autopilot_lane = lateral;
        }
        // The autopilot can't steer for the fuel pickups: it keeps the tank full, except
        // in "nofuel" mode (to test running out).
        if refuel {
            self.fuel = 300;
        }
        // Stuck behind slower traffic for a second: the next lane on the same side.
        if self.race_running && self.player_speed < 6.0 {
            p.autopilot_blocked += 1;
        } else {
            p.autopilot_blocked = 0;
        }
        if p.autopilot_blocked > RACE_AUTOPILOT_PATIENCE {
            const LANES: [f32; 3] = [30.0, 60.0, 85.0];
            let side = if p.autopilot_lane < 0.0 { -1.0 } else { 1.0 };
            let i = LANES.iter().take_while(|&&l| l < p.autopilot_lane.abs() - 1.0).count();
            p.autopilot_lane = side * LANES[(i + 1) % 3];
            p.autopilot_blocked = 0;
        }
        // Aim 160 ahead on the road at the lane's offset.
        let ahead = (self.player_chunk + if dir == 1 { 2 } else { -2 }).clamp(0, w.chunk_count - 1);
        let ah = &w.chunks[ahead as usize];
        let a = self.pools.vertices3d[w.sections[ah.section].point];
        let tx = a.x + ah.side[0] * p.autopilot_lane - self.player_x;
        let tz = a.z + ah.side[2] * p.autopilot_lane - self.player_z;
        let h = self.player_heading + 0x100;
        let cross = cos_a(h) * tz - sin_a(h) * tx;
        if cross > 2.0 { -1 } else if cross < -2.0 { 1 } else { 0 }
    }

    /// The player's steering, throttle and brake for this frame.
    pub fn handle_player_controls(&mut self) {
        if !self.player.engine_started && self.race_running {
            self.start_engine_sounds();
            self.player.engine_started = true;
        }
        if !self.controls_enabled {
            return;
        }
        // 1 keyboard, 2 joypad (its stick and buttons as the keys), 3 steering wheel (the
        // stick's x steers directly). The port falls back to the keyboard when the
        // joystick is missing.
        let (mut device, mut joystick_buttons) = (1, 0);
        let attached = self.player.joystick_attached;
        if self.player.control_type == 2 {
            device = if attached { 2 } else { 1 };
            handle_joystick();
        } else if self.player.control_type == 3 && attached {
            device = 0;
            if let Some((x, _, b)) = get_joystick_position(1, 0x1c, 0x1c) {
                self.player.steering_position = if 0x1c < x { 0x1b } else { x.max(0) };
                joystick_buttons = b;
            }
        }
        let wheel = self.player.control_type == 3 && attached;
        let steer = self.autopilot_steer();
        let look = self.camera.look_offset;
        let p = &mut self.player;
        if keyispressed_1(p.key_left, device) || joystick_buttons & 4 != 0 || steer < 0 {
            if wheel { p.steer_index = p.steering_position } else { p.update_steering(1, look) }
        } else if keyispressed_1(p.key_right, device) || joystick_buttons & 8 != 0 || steer > 0 {
            if wheel { p.steer_index = p.steering_position } else { p.update_steering(2, look) }
        } else if wheel {
            p.steer_index = p.steering_position;
        } else {
            p.update_steering(3, look);
        }

        let op = self.pools.objects3d[self.player_index].opponent as usize;
        self.opp.data[op].speed = self.player_speed;
        if self.player_speed.abs() >= 0.01 && self.player.accelerator_input != 0 {
            let s = (self.player_speed as i32).unsigned_abs() as usize;
            let turn = STEER_TABLE[self.player.steer_index as usize] / STEER_SPEED_DIVISOR[s.min(15)];
            let t = if self.player.accelerator_input == 1 { turn } else { -turn };
            self.player_heading = (self.player_heading as f32 + t) as i32;
            self.opp.data[op].speed = self.player_speed;
        }

        let p = &mut self.player;
        let autopilot_gas = p.autopilot.is_some_and(|(speed, _)| self.player_speed < speed);
        let input = if keyispressed_1(p.key_gas, device) || joystick_buttons & 1 != 0 || autopilot_gas {
            p.accelerator_input = 1;
            1
        } else if keyispressed_1(p.key_brake, device) || joystick_buttons & 2 != 0 {
            if p.rpm_step == p.idle_rpm_step {
                p.accelerator_input = 2;
            }
            match p.accelerator_input {
                1 => 2,
                2 => 1,
                _ => 0,
            }
        } else {
            3
        };
        if input != 0 {
            self.update_accelerator(input);
        }

        // Every new chunk costs fuel; going backwards gets a warning once.
        if self.player_start_lane == self.player_chunk {
            return;
        }
        self.consume_fuel(1);
        let forward = if self.track().direction == 1 {
            self.player_start_lane <= self.player_chunk
        } else {
            self.player_chunk <= self.player_start_lane
        };
        if !forward && !self.player.wrong_way_warned {
            self.player.wrong_way_warned = true;
            self.play_sound_effect(0x2f, false);
        }
        self.player_start_lane = self.player_chunk;
    }

    pub fn halt_player_car(&mut self) {
        self.player_speed = 0.0;
        let op = self.pools.objects3d[self.player_index].opponent as usize;
        self.opp.data[op].speed = 0.0;
    }

    pub fn update_player_object(&mut self, object: usize) {
        let op = &self.opp.data[self.pools.objects3d[object].opponent as usize];
        let c = &self.camera;
        let mut m = Matrix::identity();
        m.m[12] = op.x;
        m.m[13] = op.y;
        m.m[14] = op.z;
        m.rotate_y(c.car_yaw + c.look_offset);
        m.rotate_x(c.car_pitch);
        m.rotate_z(c.car_roll);
        self.pools.objects3d[object].matrix = m;
    }

    /// Creates the player's car on a chunk and lane, `offset` along the road.
    pub fn load_player_object(&mut self, direction: i32, chunk: i32, lane: i32, offset: f32) {
        let ct = &CAR_TYPES[PLAYER_CAR_TYPE];
        let data = self.files.race_file(ct.mesh).unwrap_or_else(|| panic!("Unable to load object {}", ct.mesh)).to_vec();
        self.player_index = self.pools.load_object_file(&data, ct.mesh);
        let pi = self.player_index;
        self.pools.objects3d[pi].texture = self.player.car_texture_id[PLAYER_CAR_TYPE];
        let mut m = Matrix::identity();
        m.m[14] = 1.0; // D3DMATRIX_004576d0
        self.pools.set_object3d_matrix(pi, &m);
        self.pools.objects3d[pi].update = Update::Player;
        self.build_object_execute_buffer(pi);
        self.pools.objects3d[pi].field_36 = 1;
        let op = self.alloc_opponent_slot();
        self.pools.objects3d[pi].opponent = op as i32;
        self.pools.objects3d[pi].update_interval = 1;
        let ch = &self.weg.chunks[chunk as usize];
        let c = self.pools.vertices3d[self.weg.sections[ch.section].point];
        let l = LANE_OFFSETS[lane as usize];
        self.player_y = 0.0;
        // side * lane + forward * (offset + 40); forward = (-side.z, side.x).
        self.player_x = (ch.side[0] * l + c.x) - ch.side[2] * (offset + 40.0);
        self.player_z = (offset + 40.0) * ch.side[0] + ch.side[2] * l + c.z;
        self.player_heading = self.weg.sections[chunk as usize].heading + if direction == 0 { 0x100 } else { -0x100 };
        let p = &mut self.opp.data[op];
        p.object = pi as i32;
        p.mass = 1000.0;
        p.active = true;
        p.push_x = 0.0;
        p.push_z = 0.0;
        p.offset_x = 0.0;
        p.offset_z = 0.0;
        p.waypoint = chunk;
        p.chunk = chunk;
        p.direction = direction;
        p.field_e0 = -1;
        p.field_e4 = 1;
        p.lane = lane;
        p.target_lane = lane;
        p.x = self.player_x;
        p.y = self.player_y;
        p.z = self.player_z;
        p.heading = self.player_heading;
        self.player_start_lane = chunk;
        self.player_chunk = chunk;
        self.compute_object_bounds(pi);
    }

    pub fn stop_player_car(&mut self) {
        self.halt_player_car();
        self.controls_enabled = false;
    }
}
