//! The race's sounds (sndfx.cpp, infio.cpp's effects, mycontro.cpp's engine): the
//! music, the spoken and bonus effects, the bumps and crashes by distance, the other
//! cars' engines and the player's two-layer engine with its gear changes. Volumes are
//! DirectSound's hundredths of a decibel.

use crate::race::Race;
use crate::xvga::sound::*;

/// The sample table at 0x458db4 (15 bytes a name).
const SAMPLE_NAMES: [&str; 0x30] = [
    "motor2.wav", "motor3.wav", "speed.wav", "energy.wav", "jammin.wav", "moody.wav", "disco.wav", "rock.wav",
    "sirene.wav", "bump.wav", "bump2.wav", "bump3.wav", "bump4.wav", "start.wav", "mstart.wav", "truck01.wav",
    "truck02.wav", "truck03.wav", "truck05.wav", "truck06.wav", "toet01.wav", "toet03.wav", "toet05.wav", "toet06.wav",
    "autos1.wav", "autos2.wav", "set.wav", "go.wav", "sp_b1.wav", "sp_b2.wav", "sp_b3.wav", "sp_p1.wav", "sp_p2.wav",
    "sp_p3.wav", "sp_p4.wav", "sp_p5.wav", "sp_p6.wav", "sp_p7.wav", "sp_p8.wav", "sp_p9.wav", "bonusb.wav",
    "bonusg.wav", "bonust.wav", "finish.wav", "loser.wav", "sp_r1.wav", "sp_r2.wav", "sp_r3.wav",
];

/// motor_samples: the low engine (0) and the high one (1) loop together, crossfading by
/// rpm; gearup and geardwn play on a change.
const MOTOR_SAMPLES: [&str; 4] = ["motor.wav", "motor1.wav", "gearup.wav", "geardwn.wav"];

#[derive(Clone, Copy, Default)]
struct EngineSetting {
    freq: i32,
    vol: i32,
}

pub struct Sndfx {
    pub engine_sound_enabled: bool,
    pub music_enabled: bool,
    /// From the options' volumes (init_race_settings).
    pub volume_attenuation: i32,
    pub volume_offset: i32,
    pub music_attenuation: i32,
    /// Volume by distance (|dx| + |dy| + |dz|, 0..499).
    volume_by_distance: Vec<i32>,
    /// Six engine sounds of the cars nearest the player: buffer and object (-1 free).
    object_sounds: [(i32, i32); 6],
    music_handle: i32,
    motor_handles: [i32; 4],
    /// By rpm (0..79): frequency and volume of the high engine, then of the low one.
    engine_high: Vec<EngineSetting>,
    engine_low: Vec<EngineSetting>,
}

impl Default for Sndfx {
    fn default() -> Sndfx {
        Sndfx {
            engine_sound_enabled: false,
            music_enabled: false,
            volume_attenuation: 0,
            volume_offset: 0,
            music_attenuation: 0,
            volume_by_distance: (0..500).map(|i| 0xdac - 7 * i - 4000).collect(),
            object_sounds: [(-1, -1); 6],
            music_handle: -1,
            motor_handles: [-1; 4],
            engine_high: Vec::new(),
            engine_low: Vec::new(),
        }
    }
}

impl Sndfx {
    /// mycontro.cpp: the engine's frequency and volume per rpm (init_controls).
    pub fn init_engine_sound_tables(&mut self) {
        // The high engine: below rpm 35 one Hz a step and fading in; above it 400 Hz a
        // step at a fixed volume. The low engine: fading out, gone above 35.
        self.engine_high = (0..80)
            .map(|r| match r {
                0..35 => EngineSetting { freq: 0x2b11 + r, vol: r * 0x2d - 2000 },
                _ => EngineSetting { freq: 0x2b11 + 35 + (r - 35) * 400, vol: -0x203 },
            })
            .collect();
        self.engine_low = (0..80)
            .map(|r| EngineSetting { freq: 0x157c, vol: if r < 35 { -100 - r * 0x2a } else { -5000 } })
            .collect();
    }
}

impl Race {
    /// Takes the sounds from RCS and fills the distance table (load_samples).
    pub fn load_samples(&mut self) {
        // The race's own process opened the sound with RCS in the original.
        xvga_set_sound_files(Some(self.files.rcs.clone()), 0x3c);
        self.sndfx.object_sounds = [(-1, -1); 6];
    }

    /// Stops everything (the original's process ended).
    pub fn exit_samples(&mut self) {
        stop_all_sound_buffers();
        self.sndfx.music_handle = -1;
    }

    /// One of the six tunes (index into the sample table, 2..7), looping.
    pub fn start_music(&mut self, music: usize) {
        if self.sndfx.music_enabled {
            self.sndfx.music_handle = play_sound_buffer_1(SAMPLE_NAMES[music], SOUND_LOOPING);
            set_sound_buffer_vol(self.sndfx.music_handle, -100 - self.sndfx.music_attenuation);
        }
    }

    /// Sample 0..0x2f of the table; with `wait`, until it has finished.
    pub fn play_sound_effect(&mut self, effect: usize, wait: bool) {
        if !self.sndfx.engine_sound_enabled {
            return;
        }
        assert!(effect <= 0x2f, "SNDFX sample out of bounds");
        let v = -100 - self.sndfx.volume_attenuation;
        let handle = play_sound_buffer_2(SAMPLE_NAMES[effect], v, 0);
        set_sound_buffer_vol(handle, v);
        if wait {
            while sound_buffer_busy(handle) {}
        }
    }

    fn distance_to_player(&self, x: f32, y: f32, z: f32) -> i32 {
        ((x - self.player_x).abs() + (y - self.player_y).abs() + (z - self.player_z).abs()) as i32
    }

    /// At a position, quieter further from the player and louder the harder the crash.
    pub fn play_crash_sound(&mut self, x: f32, y: f32, z: f32, speed: f32) {
        if !self.sndfx.engine_sound_enabled {
            return;
        }
        let d = self.distance_to_player(x, y, z);
        if (0..500).contains(&d) {
            let volume = self.sndfx.volume_by_distance[d as usize] - self.sndfx.volume_attenuation;
            play_sound_buffer_2("bump2.wav", volume - (speed * 25.0) as i32, 0);
        }
    }

    fn play_near(&mut self, name: &str, x: f32, y: f32, z: f32) {
        if !self.sndfx.engine_sound_enabled {
            return;
        }
        let d = self.distance_to_player(x, y, z);
        if (0..500).contains(&d) {
            play_sound_buffer_2(name, self.sndfx.volume_by_distance[d as usize] - self.sndfx.volume_attenuation, 0);
        }
    }

    pub fn play_bump3_sound(&mut self, x: f32, y: f32, z: f32) {
        self.play_near("bump3.wav", x, y, z);
    }

    pub fn play_bump_sound(&mut self, x: f32, y: f32, z: f32) {
        self.play_near("bump.wav", x, y, z);
    }

    // --- the other cars' engines -----------------------------------------------------

    /// The car type picks the sound: trucks, the police siren, or an ordinary engine.
    fn start_object_sound(&mut self, slot: usize, object: i32, distance: f32) {
        let op = self.pools.objects3d[object as usize].opponent;
        let s = &self.sndfx;
        let volume = s.volume_by_distance[distance as usize] - s.volume_attenuation;
        if !s.engine_sound_enabled {
            return;
        }
        assert!(op >= 0 && op as usize <= self.opp.count(), "start_object_sound: illegal opponent nr {op}");
        let name = match self.opp.data[op as usize].car_type {
            1 => "truck01.wav",
            5 => "truck02.wav",
            11 => "truck06.wav",
            12 => "sirene.wav",
            _ => "motor2.wav",
        };
        let handle = play_sound_buffer_2(name, volume, SOUND_LOOPING);
        set_sound_buffer_vol(handle, volume);
        self.sndfx.object_sounds[slot] = (handle, object);
    }

    pub fn stop_object_sound(&mut self, object: i32) {
        let slot = self.sndfx.object_sounds.iter().position(|s| s.1 == object);
        if let (true, Some(slot)) = (self.sndfx.engine_sound_enabled, slot) {
            stop_sound_buffer(self.sndfx.object_sounds[slot].0);
            self.sndfx.object_sounds[slot].1 = -1;
        }
    }

    /// A car's engine (or siren) for the object, at `distance` from the player.
    pub fn update_object_sound(&mut self, object: i32, _x: f32, _y: f32, _z: f32, mut distance: f32) {
        let slot = self.sndfx.object_sounds.iter().position(|s| s.1 == object);
        if !self.sndfx.engine_sound_enabled {
            return;
        }
        if !(0.0..=500.0).contains(&distance) {
            distance = 0.0;
        }
        let d = (distance as usize).min(499);
        match slot {
            None => {
                if let Some(free) = self.sndfx.object_sounds.iter().position(|s| s.1 == -1) {
                    self.start_object_sound(free, object, d as f32);
                }
            }
            Some(slot) => {
                let s = &self.sndfx;
                set_sound_buffer_vol(s.object_sounds[slot].0, s.volume_by_distance[d] - s.volume_attenuation);
            }
        }
    }

    // --- the player's engine (mycontro.cpp) ----------------------------------------------

    fn play_motor_sample(&mut self, sample: usize, flags: i32) {
        if self.sndfx.engine_sound_enabled {
            self.sndfx.motor_handles[sample] = play_sound_buffer_1(MOTOR_SAMPLES[sample], flags);
        }
    }

    fn load_and_play_motor_sample(&mut self, sample: usize, flags: i32) {
        if self.sndfx.engine_sound_enabled {
            let buffer = load_sound_buffer(MOTOR_SAMPLES[sample], SOUND_KEEP);
            self.sndfx.motor_handles[sample] = buffer;
            play_sound_buffer(buffer, flags);
        }
    }

    pub fn start_engine_sounds(&mut self) {
        if !self.sndfx.engine_sound_enabled {
            return;
        }
        self.load_and_play_motor_sample(1, SOUND_LOOPING);
        self.load_and_play_motor_sample(0, SOUND_LOOPING);
        let s = &self.sndfx;
        let (hi, lo) = (s.engine_high[0], s.engine_low[0]);
        let att = s.volume_attenuation + s.volume_offset;
        set_sound_buffer_freq(s.motor_handles[1], hi.freq + 6000);
        set_sound_buffer_vol(s.motor_handles[1], hi.vol + 300 - att);
        set_sound_buffer_vol(s.motor_handles[0], lo.vol - att);
        set_sound_buffer_freq(s.motor_handles[0], lo.freq);
    }

    /// Pitch and volume of the engine for the gearbox step's rpm, plus the throttle boost.
    pub fn set_engine_sound(&mut self, rpm: i32, boost_freq: i32, boost_vol: i32) {
        let s = &self.sndfx;
        if !s.engine_sound_enabled {
            return;
        }
        let r = rpm.clamp(0, 79) as usize;
        let (hi, lo) = (s.engine_high[r], s.engine_low[r]);
        let att = s.volume_attenuation + s.volume_offset;
        set_sound_buffer_freq(s.motor_handles[1], hi.freq + boost_freq);
        set_sound_buffer_vol(s.motor_handles[1], hi.vol + boost_vol - att);
        set_sound_buffer_freq(s.motor_handles[0], lo.freq);
        set_sound_buffer_vol(s.motor_handles[0], lo.vol - att);
    }

    pub fn play_gear_change_sound(&mut self, new_gear: i32, old_gear: i32) {
        if !self.sndfx.engine_sound_enabled {
            return;
        }
        let sample = if old_gear < new_gear { 2 } else { 3 };
        self.play_motor_sample(sample, 0);
        let s = &self.sndfx;
        set_sound_buffer_vol(s.motor_handles[sample], -100 - s.volume_attenuation - s.volume_offset);
    }
}
