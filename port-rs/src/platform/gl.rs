//! The OpenGL functions the port uses, loaded through SDL at run time so that nothing
//! links against an OpenGL library (the same code runs with Windows' opengl32, macOS'
//! OpenGL framework and Mesa). Fixed-function OpenGL 1.x/2.1, which all three offer.
#![allow(non_snake_case, non_upper_case_globals, dead_code, clippy::missing_safety_doc)]

use std::ffi::c_void;
use std::sync::OnceLock;

pub type GLenum = u32;
pub type GLint = i32;
pub type GLuint = u32;
pub type GLsizei = i32;
pub type GLfloat = f32;
pub type GLdouble = f64;
pub type GLboolean = u8;
pub type GLbitfield = u32;

pub const GL_FALSE: GLboolean = 0;
pub const GL_TRUE: GLboolean = 1;
pub const GL_DEPTH_BUFFER_BIT: GLbitfield = 0x0100;
pub const GL_COLOR_BUFFER_BIT: GLbitfield = 0x4000;
pub const GL_TRIANGLES: GLenum = 0x0004;
pub const GL_QUADS: GLenum = 0x0007;
pub const GL_NEVER: GLenum = 0x0200;
pub const GL_LESS: GLenum = 0x0201;
pub const GL_LEQUAL: GLenum = 0x0203;
pub const GL_GREATER: GLenum = 0x0204;
pub const GL_FRONT: GLenum = 0x0404;
pub const GL_BACK: GLenum = 0x0405;
pub const GL_CULL_FACE: GLenum = 0x0B44;
pub const GL_FOG: GLenum = 0x0B60;
pub const GL_DEPTH_TEST: GLenum = 0x0B71;
pub const GL_ALPHA_TEST: GLenum = 0x0BC0;
pub const GL_BLEND: GLenum = 0x0BE2;
pub const GL_SCISSOR_TEST: GLenum = 0x0C11;
pub const GL_UNPACK_ALIGNMENT: GLenum = 0x0CF5;
pub const GL_PACK_ALIGNMENT: GLenum = 0x0D05;
pub const GL_TEXTURE_2D: GLenum = 0x0DE1;
pub const GL_UNSIGNED_BYTE: GLenum = 0x1401;
pub const GL_UNSIGNED_SHORT: GLenum = 0x1403;
pub const GL_UNSIGNED_INT: GLenum = 0x1405;
pub const GL_FLOAT: GLenum = 0x1406;
pub const GL_MODELVIEW: GLenum = 0x1700;
pub const GL_PROJECTION: GLenum = 0x1701;
pub const GL_RGB: GLenum = 0x1907;
pub const GL_RGBA: GLenum = 0x1908;
pub const GL_CW: GLenum = 0x0900;
pub const GL_CCW: GLenum = 0x0901;
pub const GL_EXP: GLenum = 0x0800;
pub const GL_LINEAR: GLint = 0x2601;
pub const GL_NEAREST: GLint = 0x2600;
pub const GL_FOG_MODE: GLenum = 0x0B65;
pub const GL_FOG_DENSITY: GLenum = 0x0B62;
pub const GL_FOG_START: GLenum = 0x0B63;
pub const GL_FOG_END: GLenum = 0x0B64;
pub const GL_FOG_COLOR: GLenum = 0x0B66;
pub const GL_TEXTURE_ENV: GLenum = 0x2300;
pub const GL_TEXTURE_ENV_MODE: GLenum = 0x2200;
pub const GL_MODULATE: GLint = 0x2100;
pub const GL_REPLACE: GLint = 0x1E01;
pub const GL_TEXTURE_MAG_FILTER: GLenum = 0x2800;
pub const GL_TEXTURE_MIN_FILTER: GLenum = 0x2801;
pub const GL_TEXTURE_WRAP_S: GLenum = 0x2802;
pub const GL_TEXTURE_WRAP_T: GLenum = 0x2803;
pub const GL_REPEAT: GLint = 0x2901;
pub const GL_CLAMP_TO_EDGE: GLint = 0x812F;
pub const GL_SRC_ALPHA: GLenum = 0x0302;
pub const GL_ONE_MINUS_SRC_ALPHA: GLenum = 0x0303;
pub const GL_VERTEX_ARRAY: GLenum = 0x8074;
pub const GL_TEXTURE_COORD_ARRAY: GLenum = 0x8078;
pub const GL_RENDERER: GLenum = 0x1F01;
pub const GL_VERSION: GLenum = 0x1F02;

macro_rules! gl_functions {
    ($($name:ident($($arg:ident: $ty:ty),*) $(-> $ret:ty)?;)*) => {
        struct Functions {
            $($name: unsafe extern "system" fn($($ty),*) $(-> $ret)?,)*
        }

        static FUNCTIONS: OnceLock<Functions> = OnceLock::new();

        /// Loads the functions for the current context; the first one missing is the error.
        pub fn gl_load(get: impl Fn(&str) -> *const c_void) -> Result<(), String> {
            let f = Functions {
                $($name: {
                    let p = get(concat!("gl", stringify!($name)));
                    if p.is_null() {
                        return Err(concat!("OpenGL: gl", stringify!($name), " missing").to_string());
                    }
                    // SAFETY: the driver's function of that name has this signature.
                    unsafe { std::mem::transmute::<*const c_void, unsafe extern "system" fn($($ty),*) $(-> $ret)?>(p) }
                },)*
            };
            let _ = FUNCTIONS.set(f);
            Ok(())
        }

        $(
            #[inline]
            pub unsafe fn $name($($arg: $ty),*) $(-> $ret)? {
                (FUNCTIONS.get().expect("OpenGL not loaded").$name)($($arg),*)
            }
        )*
    };
}

gl_functions! {
    Viewport(x: GLint, y: GLint, w: GLsizei, h: GLsizei);
    Scissor(x: GLint, y: GLint, w: GLsizei, h: GLsizei);
    ClearColor(r: GLfloat, g: GLfloat, b: GLfloat, a: GLfloat);
    ClearDepth(d: GLdouble);
    Clear(mask: GLbitfield);
    Enable(cap: GLenum);
    Disable(cap: GLenum);
    Hint(target: GLenum, mode: GLenum);
    GenTextures(n: GLsizei, textures: *mut GLuint);
    DeleteTextures(n: GLsizei, textures: *const GLuint);
    BindTexture(target: GLenum, texture: GLuint);
    TexImage2D(target: GLenum, level: GLint, internalformat: GLint, w: GLsizei, h: GLsizei, border: GLint, format: GLenum, ty: GLenum, pixels: *const c_void);
    TexSubImage2D(target: GLenum, level: GLint, x: GLint, y: GLint, w: GLsizei, h: GLsizei, format: GLenum, ty: GLenum, pixels: *const c_void);
    TexParameteri(target: GLenum, pname: GLenum, param: GLint);
    TexEnvi(target: GLenum, pname: GLenum, param: GLint);
    PixelStorei(pname: GLenum, param: GLint);
    MatrixMode(mode: GLenum);
    LoadIdentity();
    LoadMatrixf(m: *const GLfloat);
    MultMatrixf(m: *const GLfloat);
    Ortho(l: GLdouble, r: GLdouble, b: GLdouble, t: GLdouble, n: GLdouble, f: GLdouble);
    Begin(mode: GLenum);
    End();
    TexCoord2f(s: GLfloat, t: GLfloat);
    Vertex2f(x: GLfloat, y: GLfloat);
    Color4f(r: GLfloat, g: GLfloat, b: GLfloat, a: GLfloat);
    EnableClientState(array: GLenum);
    DisableClientState(array: GLenum);
    VertexPointer(size: GLint, ty: GLenum, stride: GLsizei, ptr: *const c_void);
    TexCoordPointer(size: GLint, ty: GLenum, stride: GLsizei, ptr: *const c_void);
    DrawElements(mode: GLenum, count: GLsizei, ty: GLenum, indices: *const c_void);
    DrawArrays(mode: GLenum, first: GLint, count: GLsizei);
    DepthFunc(func: GLenum);
    DepthMask(flag: GLboolean);
    BlendFunc(s: GLenum, d: GLenum);
    AlphaFunc(func: GLenum, r: GLfloat);
    CullFace(mode: GLenum);
    FrontFace(mode: GLenum);
    Fogi(pname: GLenum, param: GLint);
    Fogf(pname: GLenum, param: GLfloat);
    Fogfv(pname: GLenum, params: *const GLfloat);
    ReadPixels(x: GLint, y: GLint, w: GLsizei, h: GLsizei, format: GLenum, ty: GLenum, pixels: *mut c_void);
    ReadBuffer(mode: GLenum);
    GetError() -> GLenum;
    GetString(name: GLenum) -> *const u8;
}
