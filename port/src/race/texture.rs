//! Textures (texture.cpp, material.cpp). The original loaded each PCX into a
//! DirectDraw surface with a colour key and got a Direct3D texture handle; the port
//! uploads an OpenGL texture, palette index 255 (magenta in the game's pictures)
//! transparent.

use std::ffi::c_void;

use crate::platform::gl::*;
use crate::res::archive::NamedArchive;
use crate::res::pcx::pcx_decode;

pub struct Texture {
    pub name: String,
    /// OpenGL texture name.
    pub gl: GLuint,
    pub width: i32,
    pub height: i32,
    /// Has see-through pixels.
    pub transparent: bool,
}

#[derive(Default)]
pub struct Textures {
    pub table: Vec<Texture>,
    capacity: usize,
    filter_linear: bool,
}

impl Textures {
    pub fn alloc_textures(&mut self, count: usize) {
        self.capacity = count;
        self.table.clear();
    }

    pub fn free_textures(&mut self) {
        for t in &self.table {
            // SAFETY: a texture this module created.
            unsafe { DeleteTextures(1, &t.gl) };
        }
        self.table.clear();
        self.capacity = 0;
    }

    fn apply_filter(&self) {
        let f = if self.filter_linear { GL_LINEAR } else { GL_NEAREST };
        // SAFETY: plain GL state calls on the bound texture.
        unsafe {
            TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, f);
            TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, f);
        }
    }

    /// Bilinear filtering instead of the original's nearest pixel.
    pub fn set_texture_filter(&mut self, linear: bool) {
        self.filter_linear = linear;
        for t in &self.table {
            // SAFETY: binding a texture this module created.
            unsafe { BindTexture(GL_TEXTURE_2D, t.gl) };
            self.apply_filter();
        }
    }

    /// Loads (or finds the already loaded) PCX texture; its index.
    pub fn load_texture(&mut self, files: &NamedArchive, filename: &str) -> i32 {
        if let Some(i) = self.table.iter().position(|t| t.name.eq_ignore_ascii_case(filename)) {
            return i as i32;
        }
        if self.table.len() == self.capacity {
            panic!("no more free textures ({filename})");
        }
        let img = files.find(filename).and_then(pcx_decode).unwrap_or_else(|| panic!("cannot load texture {filename}"));
        let mut transparent = false;
        let mut rgba = Vec::with_capacity(img.pixels.len() * 4);
        for &c in &img.pixels {
            rgba.extend_from_slice(&img.palette[c as usize * 3..c as usize * 3 + 3]);
            rgba.push(if c == 255 { 0 } else { 255 });
            transparent |= c == 255;
        }
        let mut gl = 0;
        // SAFETY: rgba holds width * height RGBA pixels, byte-aligned.
        unsafe {
            GenTextures(1, &mut gl);
            BindTexture(GL_TEXTURE_2D, gl);
            self.apply_filter();
            TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            PixelStorei(GL_UNPACK_ALIGNMENT, 1);
            TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA as GLint, img.width, img.height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                rgba.as_ptr() as *const c_void);
        }
        self.table.push(Texture { name: filename.to_string(), gl, width: img.width, height: img.height, transparent });
        self.table.len() as i32 - 1
    }
}
