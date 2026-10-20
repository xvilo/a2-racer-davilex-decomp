//! A port-only preview: flies along a stage's road, to check the 3D code.

use crate::game::Game;
use crate::platform::sys::*;
use crate::race::math3d::Matrix;
use crate::race::race_files::RaceFiles;
use crate::race::{Race, TRACK_INFO};

pub fn race_view(g: &Game, stage: usize) -> i32 {
    if !sys_gl() {
        eprintln!("race view: no OpenGL");
        return 1;
    }
    if stage > 5 {
        return 1;
    }
    let Some(files) = RaceFiles::race_files_open(&g.data_dir) else { return 1 };
    let mut r = Race::new(files);
    r.pools.alloc_3d_arrays(2000, 4000, 200000, 100000);
    r.pools.alloc_objects3d(800);
    r.render.textures.alloc_textures(0x50);
    r.render.change_viewport_size(0, 0, 640, 480);
    let back = r.render.textures.load_texture(&r.files.rcs, "back.pcx");
    r.render.set_background(back);
    r.weg.road_texture = r.render.textures.load_texture(&r.files.rcs, "2baans.pcx");
    r.weg.grass_texture = r.render.textures.load_texture(&r.files.rcs, "grass2.pcx");
    if !r.load_weg_data(TRACK_INFO[stage].track, 0x14) {
        return 1;
    }
    println!("race view: {}, {} chunks", TRACK_INFO[stage].track, r.weg.chunk_count);
    r.init_opponents(600);
    r.load_static_object_textures();
    r.load_static_object(0, 0.0, 0.0, 0); // the sky (with the horizon option on)
    r.init_static_objects(stage);

    let mut pos = 0.0f32; // chunks travelled
    let mut last = sys_ticks();
    let mut input = SysInput::default();
    loop {
        sys_poll(&mut input);
        if input.quit || input.key == KEY_ESCAPE {
            break;
        }
        let now = sys_ticks();
        pos += now.wrapping_sub(last) as f32 * 0.004;
        last = now;
        let mut c = pos as i32;
        if c >= r.weg.chunk_count - 2 {
            pos = 0.0;
            c = 0;
        }
        let w = &r.weg;
        let s0 = &w.sections[w.chunks[c as usize].section];
        let s1 = &w.sections[w.chunks[c as usize + 1].section];
        let (a, b) = (r.pools.vertices3d[s0.point], r.pools.vertices3d[s1.point]);
        let f = pos - c as f32;
        let side = w.chunks[c as usize].side;
        // On the right-hand carriageway, a car's height above the road.
        let x = a.x + (b.x - a.x) * f + side[0] * 60.0;
        let y = a.y + (b.y - a.y) * f;
        let z = a.z + (b.z - a.z) * f + side[2] * 60.0;
        let mut rot = Matrix::identity();
        rot.rotate_x(-20);
        rot.rotate_z(0);
        rot.rotate_y(s0.heading - 256);
        r.render.set_view(&rot, &Matrix::translation(-x, -(y + 25.0), -z));
        r.player_x = x;
        r.player_z = z;
        r.player_chunk = c;
        r.pools.check_objects3d_visibility(x as i32 - 4000, z as i32 - 4000, x as i32 + 4000, z as i32 + 4000);
        r.weg.set_visible_weg_chunks(c - 2, c + 60);
        sys_gl_begin_frame();
        r.render_3d_frame();
        sys_gl_swap();
    }
    r.render.textures.free_textures();
    0
}
