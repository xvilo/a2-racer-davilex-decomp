//! The menu's sound helpers (mysound.cpp). A handle is a sound buffer index or -1;
//! every call honours the sound option and volumes.

use crate::menu::a2types::SOUND_OFF;
use crate::menu::Menu;
use crate::platform::sys::{sys_poll, sys_sleep, SysInput};
use crate::xvga::sound::{
    get_sound_buffer_vol, max_sound_buffer_count, play_sound_buffer_1, set_sound_buffer_vol, sound_buffer_busy,
    stop_sound_buffer, SOUND_LOOPING,
};

/// The settings the helpers look at (a copy, so a handle inside `Menu` can be passed
/// along mutably).
#[derive(Clone, Copy)]
pub struct Snd {
    /// 1: no sound device.
    pub mysnd_state: i32,
    pub sound: i16,
    pub music_volume: i16,
    pub effects_volume: i16,
}

impl Menu {
    pub fn snd(&self) -> Snd {
        Snd {
            mysnd_state: self.mysnd_state,
            sound: self.opties.sound,
            music_volume: self.opties.music_volume,
            effects_volume: self.opties.effects_volume,
        }
    }
}

fn snd_valid_buffer(handle: i32) -> bool {
    handle >= 0 && (handle as usize) < max_sound_buffer_count()
}

/// The volume for an option level 0..4, in hundredths of a dB.
pub fn snd_volume_for_level(level: i16) -> i32 {
    (4 - level as i32) * -500 - 100
}

pub fn snd_stop(handle: &mut i32) {
    if snd_valid_buffer(*handle) && sound_buffer_busy(*handle) {
        stop_sound_buffer(*handle);
    }
    *handle = -1;
}

pub fn snd_play(s: Snd, handle: &mut i32, filename: &str) {
    if s.mysnd_state == 1 {
        *handle = -1;
        return;
    }
    if s.sound != SOUND_OFF {
        snd_stop(handle);
        *handle = play_sound_buffer_1(filename, 0);
        snd_set_volume(s, handle, snd_volume_for_level(s.effects_volume));
    }
}

pub fn snd_play_looping(s: Snd, handle: &mut i32, filename: &str) {
    if s.mysnd_state == 1 {
        *handle = -1;
        return;
    }
    if s.sound != SOUND_OFF {
        snd_stop(handle);
        *handle = play_sound_buffer_1(filename, SOUND_LOOPING);
        snd_set_volume(s, handle, snd_volume_for_level(s.music_volume));
    }
}

/// Waits until the sound has finished (keeping the window responsive), then stops it.
pub fn snd_wait(s: Snd, handle: &mut i32) {
    if snd_valid_buffer(*handle) && s.sound != SOUND_OFF {
        let mut input = SysInput::default();
        while sound_buffer_busy(*handle) {
            sys_poll(&mut input); // handle_win_msg
            if input.quit {
                break;
            }
            sys_sleep(5);
        }
    }
    snd_stop(handle);
}

/// One step of fading out; true while there is more to fade. (The volume is never
/// above 0, so as in the original this stops the sound at once.)
pub fn snd_fade_out(s: Snd, handle: &mut i32) -> bool {
    if snd_valid_buffer(*handle) {
        if s.sound == SOUND_OFF {
            snd_stop(handle);
        } else if sound_buffer_busy(*handle) {
            let v = get_sound_buffer_vol(*handle);
            if 0 < v - 1 {
                set_sound_buffer_vol(*handle, v - 1);
                return true;
            }
        }
    }
    snd_stop(handle);
    false
}

pub fn snd_set_volume(s: Snd, handle: &mut i32, volume: i32) -> bool {
    if snd_valid_buffer(*handle) {
        if s.sound != SOUND_OFF {
            set_sound_buffer_vol(*handle, volume);
            return true;
        }
        snd_stop(handle);
    }
    snd_stop(handle);
    false
}

#[allow(dead_code)]
pub fn snd_get_volume(s: Snd, handle: &mut i32) -> i32 {
    if snd_valid_buffer(*handle) {
        if s.sound != SOUND_OFF {
            return get_sound_buffer_vol(*handle);
        }
        snd_stop(handle);
    }
    -8000
}
