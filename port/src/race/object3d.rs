//! The 3D object pools (vertex.cpp, triangle.cpp, frame3d.cpp, anim3d.cpp, object3d.cpp,
//! objio.cpp). Everything 3D comes from these: a frame is a mesh (lists of vertex and
//! triangle indices into the pools), an anim a list of frames, an object places an
//! anim's frame in the world. The records follow the original; vertices and triangles
//! are stored as is in the .d3d model files.

use crate::race::math3d::Matrix;
use crate::race::render::Mesh;

/// A D3DVERTEX and an in-use flag (33 bytes in a .d3d file).
#[derive(Clone, Copy, Default, Debug)]
pub struct Vertex3d {
    pub x: f32,
    pub y: f32,
    pub z: f32,
    pub nx: f32,
    pub ny: f32,
    pub nz: f32,
    pub tu: f32,
    pub tv: f32,
    pub in_use: u8,
}

/// A D3DTRIANGLE (indices into the frame's vertex list) and an in-use flag (9 bytes).
#[derive(Clone, Copy, Default, Debug)]
pub struct Triangle3d {
    pub v1: u16,
    pub v2: u16,
    pub v3: u16,
    /// D3DTRIFLAG_*.
    pub flags: u16,
    pub in_use: u8,
}

#[derive(Clone, Default)]
pub struct Frame3d {
    /// Indices into vertices3d.
    pub vertices: Vec<i32>,
    /// Indices into triangles3d.
    pub triangles: Vec<i32>,
    pub in_use: bool,
}

#[derive(Clone, Default)]
pub struct Anim3d {
    /// Indices into frames3d.
    pub frames: Vec<i32>,
    pub in_use: bool,
}

/// What an object does every update_interval frames (its update function).
#[derive(Clone, Copy, Debug, PartialEq, Eq, Default)]
pub enum Update {
    #[default]
    None,
    StaticObject,
    Sky,
    /// The board "1".."6" over the racer in that place.
    PositionBoard(i32),
    Bonus,
    Pion,
    Opponent,
    Player,
}

/// object3d_t: a placed 3D object.
#[derive(Clone)]
pub struct Object3d {
    pub anim: i32,
    pub frame: i32,
    /// Position.
    pub x: f32,
    pub y: f32,
    pub z: f32,
    /// Velocity (knocked-over pionnen).
    pub vx: f32,
    pub vy: f32,
    pub vz: f32,
    pub update: Update,
    /// World matrix (a D3DMATRIXHANDLE in the original).
    pub matrix: Matrix,
    pub field_28: i32,
    /// Index into the texture table, -1 for none.
    pub texture: i32,
    /// The frame ready to draw (the execute buffer in the original).
    pub mesh: Option<Mesh>,
    pub in_use: bool,
    pub visible: bool,
    pub field_36: u8,
    /// Index into opponent_data.
    pub opponent: i32,
    pub field_3b: i32,
    pub field_3f: i32,
    pub field_43: i32,
    pub update_interval: i32,
}

impl Default for Object3d {
    fn default() -> Object3d {
        Object3d {
            anim: 0,
            frame: 0,
            x: 0.0,
            y: 0.0,
            z: 0.0,
            vx: 0.0,
            vy: 0.0,
            vz: 0.0,
            update: Update::None,
            matrix: Matrix::identity(),
            field_28: 0,
            texture: -1,
            mesh: None,
            in_use: false,
            visible: true,
            field_36: 0,
            opponent: 0,
            field_3b: 0,
            field_3f: 0,
            field_43: 0,
            update_interval: -1,
        }
    }
}

fn fatal(what: &str) -> ! {
    panic!("3d: {what}");
}

/// The pools. Each has a fixed size, as the original allocated them; new entries come
/// from the end of the used part, then from freed ones.
#[derive(Default)]
pub struct Pools {
    pub vertices3d: Vec<Vertex3d>,
    pub triangles3d: Vec<Triangle3d>,
    pub frames3d: Vec<Frame3d>,
    pub anims3d: Vec<Anim3d>,
    pub objects3d: Vec<Object3d>,
    next_vertex: usize,
    next_triangle: usize,
    next_frame3d: usize,
    next_anim3d: usize,
    next_object3d: usize,
    pub object_update_counter: i32,
}

fn next_free<T>(pool: &[T], next: &mut usize, free: impl Fn(&T) -> bool, what: &str) -> usize {
    if *next < pool.len() {
        *next += 1;
        return *next - 1;
    }
    pool.iter().position(free).unwrap_or_else(|| fatal(&format!("no more free {what}")))
}

impl Pools {
    pub fn object_count(&self) -> usize {
        self.objects3d.len()
    }

    /// alloc_3d_arrays: the anim, frame, vertex and triangle pools at once.
    pub fn alloc_3d_arrays(&mut self, anim_count: usize, frame_count: usize, vertex_count: usize, triangle_count: usize) {
        if !self.anims3d.is_empty() {
            return;
        }
        self.anims3d = vec![Anim3d::default(); anim_count];
        self.frames3d = vec![Frame3d::default(); frame_count];
        self.vertices3d = vec![Vertex3d::default(); vertex_count];
        self.triangles3d = vec![Triangle3d::default(); triangle_count];
        self.next_anim3d = 0;
        self.next_frame3d = 0;
        self.next_vertex = 0;
        self.next_triangle = 0;
    }

    pub fn free_3d_arrays(&mut self) {
        self.anims3d.clear();
        self.frames3d.clear();
        self.vertices3d.clear();
        self.triangles3d.clear();
    }

    pub fn alloc_objects3d(&mut self, count: usize) {
        if self.objects3d.is_empty() {
            self.objects3d = vec![Object3d::default(); count];
            self.next_object3d = 0;
        }
    }

    pub fn free_objects3d(&mut self) {
        self.objects3d.clear();
        self.next_object3d = 0;
    }

    pub fn get_new_vertices(&mut self) -> usize {
        next_free(&self.vertices3d, &mut self.next_vertex, |v| v.in_use == 0, "vertices")
    }

    pub fn get_new_triangles(&mut self) -> usize {
        next_free(&self.triangles3d, &mut self.next_triangle, |t| t.in_use == 0, "triangles")
    }

    pub fn init_frame3d(&mut self, vertex_count: usize, triangle_count: usize) -> i32 {
        let f = next_free(&self.frames3d, &mut self.next_frame3d, |f| !f.in_use, "frames");
        let mut vertices = Vec::with_capacity(vertex_count);
        for _ in 0..vertex_count {
            let v = self.get_new_vertices();
            self.vertices3d[v].in_use = 1;
            vertices.push(v as i32);
        }
        let mut triangles = Vec::with_capacity(triangle_count);
        for _ in 0..triangle_count {
            let t = self.get_new_triangles();
            self.triangles3d[t].in_use = 1;
            triangles.push(t as i32);
        }
        self.frames3d[f] = Frame3d { vertices, triangles, in_use: true };
        f as i32
    }

    #[allow(dead_code)]
    pub fn free_frame3d(&mut self, frame: i32) {
        let fr = std::mem::take(&mut self.frames3d[frame as usize]);
        if !fr.in_use {
            return;
        }
        for &v in &fr.vertices {
            self.vertices3d[v as usize].in_use = 0;
        }
        for &t in &fr.triangles {
            self.triangles3d[t as usize].in_use = 0;
        }
    }

    fn get_new_anim3d(&mut self) -> usize {
        next_free(&self.anims3d, &mut self.next_anim3d, |a| !a.in_use, "anims")
    }

    /// An anim whose frames are allocated straight away, all the same size.
    pub fn init_static_anim3d(&mut self, frame_count: usize, vertex_count: usize, triangle_count: usize) -> i32 {
        let a = self.get_new_anim3d();
        let frames = (0..frame_count).map(|_| self.init_frame3d(vertex_count, triangle_count)).collect();
        self.anims3d[a] = Anim3d { frames, in_use: true };
        a as i32
    }

    /// An anim whose frames are filled in later (load_object_mesh).
    pub fn init_anim3d(&mut self, frame_count: usize) -> i32 {
        let a = self.get_new_anim3d();
        self.anims3d[a] = Anim3d { frames: vec![0; frame_count], in_use: true };
        a as i32
    }

    pub fn get_new_object3d(&mut self) -> usize {
        next_free(&self.objects3d, &mut self.next_object3d, |o| !o.in_use, "objects")
    }

    pub fn set_object3d_matrix(&mut self, object: usize, m: &Matrix) {
        if object >= self.objects3d.len() {
            fatal("illegal object in set_object3d_matrix");
        }
        self.objects3d[object].matrix = *m;
    }

    /// Objects outside the rectangle (x, z) are hidden.
    pub fn check_objects3d_visibility(&mut self, min_x: i32, min_z: i32, max_x: i32, max_z: i32) {
        for o in self.objects3d.iter_mut() {
            o.visible = o.in_use
                && !(o.x < min_x as f32 || (max_x as f32) < o.x || o.z < min_z as f32 || (max_z as f32) < o.z);
        }
    }

    /// The current frame of an object.
    pub fn object_frame(&self, object: usize) -> &Frame3d {
        let o = &self.objects3d[object];
        &self.frames3d[self.anims3d[o.anim as usize].frames[o.frame as usize] as usize]
    }

    /// The frame index an object shows.
    pub fn object_frame_index(&self, object: usize) -> i32 {
        let o = &self.objects3d[object];
        self.anims3d[o.anim as usize].frames[o.frame as usize]
    }

    /// Fills a frame with a .d3d model's vertices and triangles.
    fn read_mesh(&mut self, data: &[u8], frame: i32, filename: &str) {
        let fr = self.frames3d[frame as usize].clone();
        let (nv, nt) = (fr.vertices.len(), fr.triangles.len());
        if data.len() < 8 + nv * 33 + nt * 9 {
            panic!("{filename}: file too short");
        }
        let f32_at = |p: usize| f32::from_le_bytes([data[p], data[p + 1], data[p + 2], data[p + 3]]);
        let u16_at = |p: usize| u16::from_le_bytes([data[p], data[p + 1]]);
        let mut p = 8;
        for &v in &fr.vertices {
            self.vertices3d[v as usize] = Vertex3d {
                x: f32_at(p),
                y: f32_at(p + 4),
                z: f32_at(p + 8),
                nx: f32_at(p + 12),
                ny: f32_at(p + 16),
                nz: f32_at(p + 20),
                tu: f32_at(p + 24),
                tv: f32_at(p + 28),
                in_use: data[p + 32],
            };
            p += 33;
        }
        for &t in &fr.triangles {
            self.triangles3d[t as usize] = Triangle3d {
                v1: u16_at(p),
                v2: u16_at(p + 2),
                v3: u16_at(p + 4),
                flags: u16_at(p + 6),
                in_use: data[p + 8],
            };
            p += 9;
        }
    }

    /// objio.cpp: a .d3d model file (vertex count, triangle count, the vertices, the
    /// triangles) as a new object.
    pub fn load_object_file(&mut self, data: &[u8], filename: &str) -> usize {
        let (nv, nt) = model_counts(data, filename);
        let o = self.get_new_object3d();
        self.objects3d[o].in_use = true;
        self.objects3d[o].anim = self.init_static_anim3d(1, nv, nt);
        self.objects3d[o].frame = 0;
        let frame = self.object_frame_index(o);
        self.read_mesh(data, frame, filename);
        o
    }

    /// A model into an existing object's current frame.
    pub fn load_object_mesh(&mut self, object: usize, data: &[u8], filename: &str) {
        let (nv, nt) = model_counts(data, filename);
        self.objects3d[object].in_use = true;
        let f = self.init_frame3d(nv, nt);
        let (anim, frame) = (self.objects3d[object].anim as usize, self.objects3d[object].frame as usize);
        self.anims3d[anim].frames[frame] = f;
        self.read_mesh(data, f, filename);
    }
}

fn model_counts(data: &[u8], filename: &str) -> (usize, usize) {
    if data.len() < 8 {
        panic!("Unable to load object {filename}");
    }
    let n = |p: usize| i32::from_le_bytes([data[p], data[p + 1], data[p + 2], data[p + 3]]).max(0) as usize;
    (n(0), n(4))
}
