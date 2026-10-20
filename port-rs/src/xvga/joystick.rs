//! Joysticks (joystick.cpp) and keyispressed_1 (key.cpp). Up to two joysticks (1, 2),
//! each turning its stick and first four buttons into eight "keys": left, right, up,
//! down, button 1..4, each mapped to a virtual-key code the game asks about.

use std::cell::RefCell;

use crate::platform::sys::{keyispressed, sys_joystick_position};

/// joystickstate: 0x5c bytes a joystick; entry 0 is unused.
#[derive(Default, Clone, Copy)]
struct JoystickState {
    attached: bool,
    /// (key, down) per slot.
    keys: [(i32, bool); 8],
    /// The centre.
    x_base: i32,
    y_base: i32,
    x_max: i32,
    x_min: i32,
    y_max: i32,
    y_min: i32,
}

struct Joysticks {
    state: [JoystickState; 3],
    sencesibility: f64,
}

thread_local! {
    static JOY: RefCell<Joysticks> = RefCell::new(Joysticks { state: [JoystickState::default(); 3], sencesibility: 0.1 });
}

/// Bit 0: joystick 1 attached, bit 1: joystick 2.
pub fn get_attached_joysticks() -> i32 {
    let a1 = sys_joystick_position(0).is_some();
    let a2 = sys_joystick_position(1).is_some();
    JOY.with(|j| {
        let mut j = j.borrow_mut();
        j.state[1].attached = a1;
        j.state[2].attached = a2;
    });
    a1 as i32 + 2 * a2 as i32
}

/// Takes each joystick's present position as its centre.
pub fn calibrate_joysticks() {
    for n in 1..=2 {
        let pos = sys_joystick_position(n - 1);
        JOY.with(|j| {
            let s = &mut j.borrow_mut().state[n];
            s.attached = pos.is_some();
            if let Some((x, y, _)) = pos {
                s.x_base = x as i32;
                s.y_base = y as i32;
            }
            // joyGetDevCaps: SDL's axes give the whole range.
            s.x_min = 0;
            s.y_min = 0;
            s.x_max = 0xffff;
            s.y_max = 0xffff;
        });
    }
}

fn check_joystick_movement(j: &mut Joysticks, n: usize, x: i32, y: i32, buttons: u32) {
    let sens = j.sencesibility;
    let s = &mut j.state[n];
    for k in s.keys.iter_mut() {
        k.1 = false;
    }
    let (xb, yb) = (s.x_base as f64, s.y_base as f64);
    if xb * sens + xb < x as f64 {
        s.keys[1].1 = true;
    }
    if (x as f64) < xb - xb * sens {
        s.keys[0].1 = true;
    }
    if yb * sens + yb < y as f64 {
        s.keys[3].1 = true;
    }
    if (y as f64) < yb - yb * sens {
        s.keys[2].1 = true;
    }
    for i in 0..4 {
        if buttons & 1 << i != 0 {
            s.keys[4 + i].1 = true;
        }
    }
}

/// Reads the joysticks into their key states.
pub fn handle_joystick() {
    for n in 1..=2 {
        if !JOY.with(|j| j.borrow().state[n].attached) {
            continue;
        }
        let pos = sys_joystick_position(n - 1);
        JOY.with(|j| {
            let mut j = j.borrow_mut();
            match pos {
                Some((x, y, b)) => check_joystick_movement(&mut j, n, x as i32, y as i32, b),
                None => j.state[n].keys.iter_mut().for_each(|k| k.1 = false),
            }
        });
    }
}

/// Joystick 1 or 2, slot 0..7 (left, right, up, down, buttons 1..4), the key it stands for.
pub fn set_joystick_keys(joystick: usize, slot: usize, key: i32) {
    assert!((1..=2).contains(&joystick), "set_joystick_keys: invalid joystick {joystick}");
    assert!(slot < 8, "set_joystick_keys: invalid joystick key {slot}");
    JOY.with(|j| j.borrow_mut().state[joystick].keys[slot].0 = key);
}

/// The dead zone, a fraction (0.05..1) of the centre value.
#[allow(dead_code)]
pub fn set_joystick_sensibility(sensibility: f64) {
    assert!((0.05..=1.0).contains(&sensibility), "set_joystick_sencesibility: invalid value");
    JOY.with(|j| j.borrow_mut().sencesibility = sensibility);
}

/// The stick scaled to 0..width and 0..height (the centre in the dead zone), and
/// buttons: 1 up, 2 down, 4 left, 8 right; None when there is no joystick.
pub fn get_joystick_position(joystick: usize, width: i32, height: i32) -> Option<(i32, i32, i32)> {
    assert!((1..=2).contains(&joystick), "get_joystick_position: invalid joystick {joystick}");
    let (half_w, half_h) = (width / 2, height / 2);
    let (s, sens) = JOY.with(|j| (j.borrow().state[joystick], j.borrow().sencesibility));
    if !s.attached {
        return None;
    }
    let (jx, jy, _) = sys_joystick_position(joystick - 1)?;
    let mut buttons = 0;
    let (c, d) = (s.x_base as f64, s.x_base as f64 * sens);
    let x = if c + d < jx as f64 {
        buttons |= 8;
        (jx / ((s.x_max - s.x_base) / half_w).max(1) as u32) as i32
    } else if (jx as f64) < c - d {
        buttons |= 4;
        (jx / ((s.x_base - s.x_min) / half_w).max(1) as u32) as i32
    } else {
        half_w
    };
    let (c, d) = (s.y_base as f64, s.y_base as f64 * sens);
    let y = if c + d < jy as f64 {
        buttons |= 2;
        (jy / ((s.y_max - s.y_base) / half_h).max(1) as u32) as i32
    } else if (jy as f64) < c - d {
        buttons |= 1;
        (jy / ((s.y_base - s.y_min) / half_h).max(1) as u32) as i32
    } else {
        half_h
    };
    Some((x, y, buttons))
}

/// Whether a joystick key standing for the key is down.
pub fn check_joystick_keys(key: i32) -> bool {
    JOY.with(|j| j.borrow().state[1..].iter().any(|s| s.keys.iter().any(|&(k, down)| k == key && down)))
}

/// The key on a device: bit 0 the keyboard, bit 1 the joysticks.
pub fn keyispressed_1(key: i32, device: i32) -> bool {
    assert!((0..=3).contains(&device), "keyispressed: illegal device request {device}");
    (device & 2 != 0 && check_joystick_keys(key)) || (device & 1 != 0 && keyispressed(key))
}
