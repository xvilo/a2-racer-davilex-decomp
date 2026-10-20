//! The race (A2SPEL.DAT). Its state lives in `Race`; each original source file is a
//! module adding its part of the state and its functions, under the original's names.

pub mod bonus;
pub mod camera;
pub mod driver;
pub mod hud;
pub mod mapexec;
pub mod math3d;
pub mod object3d;
pub mod opponent;
pub mod pion;
pub mod player;
pub mod race;
pub mod race_files;
pub mod race_view;
pub mod render;
pub mod sndfx;
pub mod statobj;
pub mod texture;
pub mod weg;

use crate::menu::infio::Infio;
use object3d::{Pools, Update};
use race_files::RaceFiles;
use render::{Mesh, Render};
use statobj::{StaticObjectType, STATIC_OBJECT_TYPES};

/// Race states (race_state; set_race_state).
pub const RACE_RUNNING: i32 = 0;
/// The camera comes down from above.
pub const RACE_INTRO: i32 = 1;
/// 3, 2, 1, go.
pub const RACE_COUNTDOWN: i32 = 2;
/// The player crossed the line.
pub const RACE_FINISHED: i32 = 3;
/// Caught by the police: the fine is shown.
pub const RACE_FINE: i32 = 4;
pub const RACE_OUT_OF_TIME: i32 = 5;
pub const RACE_NO_MONEY: i32 = 6;
pub const RACE_OUT_OF_FUEL: i32 = 7;
pub const RACE_EXIT: i32 = 8;
pub const RACE_PAUSED: i32 = 9;

/// track_info in A2SPEL.DAT, per stage.
#[derive(Clone, Copy)]
pub struct TrackInfo {
    pub music: usize,
    pub track: &'static str,
    /// 1: from the track's first point to its last; 0: back.
    pub direction: i32,
    /// Seconds (TRACK/START1..3 in A2.INI replace them).
    pub time_limit: i32,
}

pub const TRACK_INFO: [TrackInfo; 6] = [
    TrackInfo { music: 2, track: "etappe1.trk", direction: 1, time_limit: 600 },
    TrackInfo { music: 3, track: "etappe2.trk", direction: 1, time_limit: 900 },
    TrackInfo { music: 4, track: "etappe3.trk", direction: 1, time_limit: 1200 },
    TrackInfo { music: 5, track: "etappe3.trk", direction: 0, time_limit: 1200 },
    TrackInfo { music: 6, track: "etappe2.trk", direction: 0, time_limit: 900 },
    TrackInfo { music: 7, track: "etappe1.trk", direction: 0, time_limit: 600 },
];

/// Racers' start and finish times (racer_finish_times: time, start, place).
#[derive(Clone, Copy, Default)]
pub struct RacerTime {
    pub finish_time: i32,
    pub start: i32,
    pub place: i32,
}

pub struct Race {
    pub files: RaceFiles,
    pub infio: Infio,
    pub pools: Pools,
    pub render: Render,
    pub weg: weg::Weg,
    pub mapexec: mapexec::MapExec,
    pub static_object_types: [StaticObjectType; STATIC_OBJECT_TYPES],
    pub camera: camera::Camera,
    pub player: player::Player,
    pub opp: opponent::Opponents,
    pub drivers: driver::Drivers,
    pub bonus: bonus::Bonus,
    pub pions: pion::Pions,
    pub hud: hud::Hud,
    pub sndfx: sndfx::Sndfx,
    /// rand()'s state (Watcom's generator, seeded from the clock as WinMain did).
    rand_next: u32,

    // main.cpp, objio.cpp, setracer.cpp
    pub track_info: [TrackInfo; 6],
    pub track_index: usize,
    pub player_x: f32,
    pub player_y: f32,
    pub player_z: f32,
    pub player_speed: f32,
    pub player_chunk: i32,
    pub player_heading: i32,
    /// The player's object.
    pub player_index: usize,
    /// The chunk the player was on (fuel use, wrong-way).
    pub player_start_lane: i32,
    pub race_state: i32,
    pub race_running: bool,
    pub controls_enabled: bool,
    pub race_finished_ok: bool,
    /// 0: 640x480, 1: 320x200.
    pub screen_resolution: i32,
    pub dashboard_on_screen: bool,
    pub horizon_on: bool,
    pub detail_level: i32,
    pub police_count: usize,
    pub civilian_count: usize,
    pub money: i32,
    pub fuel: i32,
    pub time_left: i32,
    texture_filtering: bool,
    countdown_seconds: i32,
    countdown_last_tick: i32,
    countdown_step: i32,
    intro_started: bool,
    countdown_started: bool,
    finish_started: bool,
    finish_frame: i32,
    time_clock: i32,
    fine_count: usize,
    current_fine: i32,
    racer_times: [RacerTime; 7],
}

impl Race {
    pub fn new(files: RaceFiles) -> Race {
        let seed = std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).map(|d| d.as_secs() as u32).unwrap_or(1);
        Race {
            files,
            infio: Infio::default(),
            pools: Pools::default(),
            render: Render::default(),
            weg: Default::default(),
            mapexec: Default::default(),
            static_object_types: [StaticObjectType { texture: -1, mesh: -1 }; STATIC_OBJECT_TYPES],
            camera: Default::default(),
            player: Default::default(),
            opp: Default::default(),
            drivers: Default::default(),
            bonus: Default::default(),
            pions: Default::default(),
            hud: Default::default(),
            sndfx: Default::default(),
            rand_next: seed,
            track_info: TRACK_INFO,
            track_index: 0,
            player_x: 0.0,
            player_y: 0.0,
            player_z: 0.0,
            player_speed: 0.0,
            player_chunk: 0,
            player_heading: 0,
            player_index: 0,
            player_start_lane: 0,
            race_state: RACE_INTRO,
            race_running: false,
            controls_enabled: false,
            race_finished_ok: false,
            screen_resolution: 0,
            dashboard_on_screen: false,
            horizon_on: false,
            detail_level: 0,
            police_count: 8,
            civilian_count: 150,
            money: 0,
            fuel: 0,
            time_left: 0,
            texture_filtering: false,
            countdown_seconds: 0,
            countdown_last_tick: 0,
            countdown_step: 0,
            intro_started: false,
            countdown_started: false,
            finish_started: false,
            finish_frame: 0,
            time_clock: 0,
            fine_count: 0,
            current_fine: 0,
            racer_times: [RacerTime::default(); 7],
        }
    }

    /// The C library's rand() as Watcom built it: 0..32767.
    pub fn rand(&mut self) -> i32 {
        self.rand_next = self.rand_next.wrapping_mul(0x41c6_4e6d).wrapping_add(0x3039);
        ((self.rand_next >> 16) & 0x7fff) as i32
    }

    pub fn track(&self) -> TrackInfo {
        self.track_info[self.track_index]
    }

    /// build_object_execute_buffer: the object's current frame as a mesh.
    pub fn build_object_execute_buffer(&mut self, object: usize) {
        let fr = self.pools.object_frame(object);
        let mut m = Mesh::new(fr.vertices.len(), fr.triangles.len());
        for (i, &v) in fr.vertices.iter().enumerate() {
            m.set_vertex(i, &self.pools.vertices3d[v as usize]);
        }
        m.texture(self.pools.objects3d[object].texture);
        for &t in &fr.triangles {
            let t = self.pools.triangles3d[t as usize];
            m.triangle(t.v1 as usize, t.v2 as usize, t.v3 as usize);
        }
        self.pools.objects3d[object].mesh = Some(m);
    }

    /// An object's update function.
    fn run_update(&mut self, object: usize, update: Update) {
        match update {
            Update::None => {}
            Update::StaticObject => self.update_static_object(object),
            Update::Sky => self.update_sky_object(object),
            Update::PositionBoard(place) => self.update_position_board(object, place),
            Update::Bonus => self.update_bonus_object(object),
            Update::Pion => self.update_pion_object(object),
            Update::Opponent => self.update_opponent_object(object),
            Update::Player => self.update_player_object(object),
        }
    }

    /// Runs the objects' update functions (each every update_interval calls).
    pub fn execute_object_buffers(&mut self) {
        for i in 0..self.pools.object_count() {
            let o = &self.pools.objects3d[i];
            if !o.in_use || o.update_interval <= 0 {
                continue;
            }
            // One counter for all objects, as in the original.
            let counter = self.pools.object_update_counter;
            self.pools.object_update_counter = counter.wrapping_add(1);
            let (interval, update) = (o.update_interval, o.update);
            if counter % interval == 0 {
                self.run_update(i, update);
            }
        }
    }

    /// Draws the background, the visible objects and the road (render_3d_frame).
    pub fn render_3d_frame(&mut self) {
        self.execute_object_buffers();
        self.render.begin_scene();
        for o in &self.pools.objects3d {
            if o.in_use && o.visible {
                if let Some(m) = &o.mesh {
                    self.render.mesh_draw(m, Some(&o.matrix));
                }
            }
        }
        self.draw_weg();
        self.render.end_scene();
    }
}
