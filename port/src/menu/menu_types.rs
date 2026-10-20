//! Menu table types. The layouts follow A2MENU.DAT's structures (symbols/structs.tsv),
//! without the original's packing or its run-time window handles. Pointers between
//! tables become a table name and an index (see menu_data.rs).

pub use crate::menu::menu_data::{AreaButtonTable, ButtonTable, EditBoxTable, FrameTable};

pub type Text = Option<&'static [u8]>;

#[derive(Clone, Copy, Debug)]
pub struct Frame {
    /// Ticks the frame stays up.
    pub duration: i32,
    /// Sprite index in A2MRC.
    pub image: i32,
}

#[derive(Clone, Copy, Debug)]
pub struct Anim {
    pub frame_count: i32,
    pub mode: i32,
    pub frames: (FrameTable, usize),
}

/// Control kinds in Form::ty.
pub const CONTROL_EDITBOX: i32 = 0;
pub const CONTROL_BUTTON: i32 = 1;
pub const CONTROL_EDITBOX_READONLY: i32 = 3;
pub const CONTROL_AREABUTTON: i32 = 4;

/// The kind's own record of a form row.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Ctl {
    None,
    Button(ButtonTable, usize),
    AreaButton(AreaButtonTable, usize),
    EditBox(EditBoxTable, usize),
}

/// One control on a form; a table ends with a row whose x is -1.
#[derive(Clone, Copy, Debug)]
pub struct Form {
    pub x: i32,
    pub y: i32,
    pub ty: i32,
    pub control: Ctl,
    /// Initial control state.
    pub state: i32,
    pub field_14: i32,
}

#[derive(Clone, Copy, Debug)]
pub struct Button {
    /// Anim index; frame 0 normal, 1 pressed, 2 disabled, 3 focused.
    pub image: i32,
    /// -1: from the image.
    pub width: i32,
    pub height: i32,
    pub hotkey: u8,
    /// Anim played while focused, -1 for none.
    pub focus_anim: i32,
    pub focus_frame: i32,
}

/// A clickable area, e.g. a save slot row (areabutton_t).
#[derive(Clone, Copy, Debug)]
pub struct AreaButton {
    /// -1: from the image.
    pub width: i32,
    pub height: i32,
    /// Anim drawn when the form opens, -1 for none.
    pub image: i32,
    pub field_0c: i32,
    pub hotkey: u8,
}

/// A text entry field (edit_box_t). Colours are COLORREFs; create_edit_box sets them.
#[derive(Clone, Copy, Debug)]
pub struct EditBox {
    pub width: i32,
    pub height: i32,
    pub max_chars: i32,
    /// 1: single line.
    pub lines: i32,
    /// Initial text.
    pub text: Text,
    pub background: u32,
    pub foreground: u32,
    pub focus_background: u32,
    pub focus_foreground: u32,
}

/// Screen position of a save slot row (mst_slot_t, mns_slot_t).
#[derive(Clone, Copy, Debug)]
pub struct SlotPos {
    pub x: i32,
    pub y: i32,
}

/// A key that can be assigned, with its name (key_list).
#[derive(Clone, Copy, Debug)]
pub struct Keyset {
    /// Windows virtual-key code.
    pub vk: i32,
    pub name: Text,
}

/// One key setting on the controls screen (config_key).
#[derive(Clone, Copy, Debug)]
pub struct ConfigKey {
    pub x: i32,
    pub y: i32,
    pub x2: i32,
    pub y2: i32,
    pub label: Text,
    pub key: i32,
}

/// One high-score entry (SCORE.A2R), laid out as in the file.
#[derive(Clone, Copy, Debug)]
pub struct SingleScore {
    pub name: [u8; 32],
    /// Seconds.
    pub time: i32,
    /// Last stage reached (overall list).
    pub stage: i16,
    /// 1 for the entry added last.
    pub is_new: i16,
    pub money: i32,
}

/// Help text for a control; a table ends with control_id -1.
#[derive(Clone, Copy, Debug)]
pub struct Help {
    pub control_id: i32,
    pub text: Text,
}

/// A NUL-padded 32-byte name.
pub fn name32(s: &[u8]) -> [u8; 32] {
    let mut out = [0u8; 32];
    let n = s.len().min(31);
    out[..n].copy_from_slice(&s[..n]);
    out
}

/// The text of a NUL-terminated byte buffer.
pub fn cstr(buf: &[u8]) -> &[u8] {
    &buf[..buf.iter().position(|&c| c == 0).unwrap_or(buf.len())]
}
