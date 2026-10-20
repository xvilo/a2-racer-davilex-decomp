//! Drawing the 3D scene (d3dlib.cpp, viewport.cpp, exec.cpp). The original built a
//! Direct3D execute buffer per object and per road chunk; the port keeps a mesh with
//! the same vertices and triangles and draws it with OpenGL.
//!
//! The original's settings, kept: no lighting (vertices are drawn white, the texture as
//! is), nearest-pixel textures, z-buffer, back faces (counter-clockwise on screen)
//! culled, and its projection: vertically tan(fov/2) = 0.25, horizontally that times the
//! viewport's aspect, near plane at 1 and no far plane.

use std::ffi::c_void;

use crate::platform::gl::*;
use crate::platform::sys::sys_gl_viewport;
use crate::race::math3d::Matrix;
use crate::race::object3d::Vertex3d;
use crate::race::texture::Textures;

#[derive(Clone, Copy, Default)]
#[repr(C)]
pub struct MeshVertex {
    pub x: f32,
    pub y: f32,
    pub z: f32,
    pub u: f32,
    pub v: f32,
}

/// Triangles drawn with one texture: a range of indices.
#[derive(Clone, Copy)]
struct Batch {
    texture: i32,
    first: usize,
    count: usize,
}

#[derive(Clone, Default)]
pub struct Mesh {
    pub vertices: Vec<MeshVertex>,
    indices: Vec<u16>,
    index_capacity: usize,
    batches: Vec<Batch>,
}

impl Mesh {
    pub fn new(vertex_count: usize, triangle_capacity: usize) -> Mesh {
        Mesh {
            vertices: vec![MeshVertex::default(); vertex_count],
            indices: Vec::with_capacity(triangle_capacity * 3),
            index_capacity: triangle_capacity * 3,
            batches: Vec::new(),
        }
    }

    pub fn set_vertex(&mut self, i: usize, v: &Vertex3d) {
        self.vertices[i] = MeshVertex { x: v.x, y: v.y, z: v.z, u: v.tu, v: v.tv };
    }

    /// Following triangles use this texture (-1: untextured).
    pub fn texture(&mut self, texture: i32) {
        if self.batches.len() == 4 {
            panic!("mesh: too many textures");
        }
        self.batches.push(Batch { texture, first: self.indices.len(), count: 0 });
    }

    pub fn triangle(&mut self, a: usize, b: usize, c: usize) {
        if self.batches.is_empty() {
            self.texture(-1);
        }
        let n = self.vertices.len();
        if self.indices.len() + 3 > self.index_capacity || a >= n || b >= n || c >= n {
            panic!("mesh: triangle out of range");
        }
        self.indices.extend_from_slice(&[a as u16, b as u16, c as u16]);
        self.batches.last_mut().unwrap().count += 3;
    }
}

pub struct Render {
    /// change_viewport_size: the 3D picture's rectangle in the 640x480 frame.
    pub viewport: (i32, i32, i32, i32),
    background_texture: i32,
    view_rotation: Matrix,
    view_translation: Matrix,
    pub textures: Textures,
}

impl Default for Render {
    fn default() -> Render {
        Render {
            viewport: (0, 0, 640, 480),
            background_texture: -1,
            view_rotation: Matrix::identity(),
            view_translation: Matrix::identity(),
            textures: Textures::default(),
        }
    }
}

impl Render {
    pub fn change_viewport_size(&mut self, x: i32, y: i32, w: i32, h: i32) {
        self.viewport = (x, y, w, h);
    }

    pub fn set_background(&mut self, texture: i32) {
        self.background_texture = texture;
    }

    /// set_camera's two matrices: the rotation and the (negated) camera position.
    pub fn set_view(&mut self, rotation: &Matrix, translation: &Matrix) {
        self.view_rotation = *rotation;
        self.view_translation = *translation;
    }

    pub fn mesh_draw(&self, m: &Mesh, world: Option<&Matrix>) {
        if m.indices.is_empty() {
            return;
        }
        // modelview = world * view: v * World * View, as Direct3D applies them.
        let mut mv = self.view_rotation;
        mv.multiply(&self.view_translation);
        if let Some(w) = world {
            mv.multiply(w);
        }
        let stride = std::mem::size_of::<MeshVertex>() as GLsizei;
        // SAFETY: the pointers stay valid during the draw calls; indices are in range
        // (Mesh::triangle checks them).
        unsafe {
            MatrixMode(GL_MODELVIEW);
            LoadMatrixf(mv.m.as_ptr());
            VertexPointer(3, GL_FLOAT, stride, m.vertices.as_ptr() as *const c_void);
            TexCoordPointer(2, GL_FLOAT, stride, (&m.vertices[0].u) as *const f32 as *const c_void);
            for b in &m.batches {
                if b.count == 0 {
                    continue;
                }
                if b.texture >= 0 {
                    Enable(GL_TEXTURE_2D);
                    BindTexture(GL_TEXTURE_2D, self.textures.table[b.texture as usize].gl);
                } else {
                    Disable(GL_TEXTURE_2D);
                }
                DrawElements(GL_TRIANGLES, b.count as GLsizei, GL_UNSIGNED_SHORT, m.indices[b.first..].as_ptr() as *const c_void);
            }
        }
    }

    /// The viewport's background material: the picture stretched over the 3D area.
    fn draw_background(&self) {
        if self.background_texture < 0 {
            return;
        }
        // SAFETY: plain GL state and immediate-mode calls.
        unsafe {
            Disable(GL_DEPTH_TEST);
            Disable(GL_CULL_FACE);
            MatrixMode(GL_PROJECTION);
            LoadIdentity();
            Ortho(0.0, 1.0, 1.0, 0.0, -1.0, 1.0);
            MatrixMode(GL_MODELVIEW);
            LoadIdentity();
            Enable(GL_TEXTURE_2D);
            BindTexture(GL_TEXTURE_2D, self.textures.table[self.background_texture as usize].gl);
            Begin(GL_QUADS);
            for (u, v) in [(0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0)] {
                TexCoord2f(u, v);
                Vertex2f(u, v);
            }
            End();
        }
    }

    fn set_projection(&self) {
        // Direct3D's projection [2 0 0 0; 0 2 0 0; 0 0 1 1; 0 0 -1 0] with the
        // viewport's scale (its height in pixels), as OpenGL clip coordinates (z from -1).
        let aspect = self.viewport.2 as f32 / self.viewport.3 as f32;
        let mut p = [0.0f32; 16];
        p[0] = 4.0 / aspect;
        p[5] = 4.0;
        p[10] = 1.0;
        p[11] = 1.0;
        p[14] = -2.0;
        // SAFETY: plain GL state calls.
        unsafe {
            MatrixMode(GL_PROJECTION);
            LoadMatrixf(p.as_ptr());
        }
    }

    /// The first half of render_3d_frame: the background and the 3D state.
    pub fn begin_scene(&self) {
        let (x, y, w, h) = self.viewport;
        sys_gl_viewport(x, y, w, h);
        // SAFETY: plain GL state calls.
        unsafe { Clear(GL_DEPTH_BUFFER_BIT) };
        self.draw_background();
        self.set_projection();
        // SAFETY: plain GL state calls.
        unsafe {
            Enable(GL_DEPTH_TEST);
            DepthFunc(GL_LEQUAL);
            DepthMask(GL_TRUE);
            Enable(GL_CULL_FACE);
            FrontFace(GL_CW);
            CullFace(GL_BACK);
            Enable(GL_ALPHA_TEST);
            AlphaFunc(GL_GREATER, 0.5);
            TexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
            EnableClientState(GL_VERTEX_ARRAY);
            EnableClientState(GL_TEXTURE_COORD_ARRAY);
        }
    }

    /// The end of render_3d_frame: the state back for the 2D overlay.
    pub fn end_scene(&self) {
        // SAFETY: plain GL state calls.
        unsafe {
            DisableClientState(GL_VERTEX_ARRAY);
            DisableClientState(GL_TEXTURE_COORD_ARRAY);
            Disable(GL_ALPHA_TEST);
            Disable(GL_CULL_FACE);
            Disable(GL_DEPTH_TEST);
            Disable(GL_TEXTURE_2D);
        }
    }
}
