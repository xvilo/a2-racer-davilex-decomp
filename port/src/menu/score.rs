//! High scores (score.cpp), kept in SCORE.A2R: the overall top ten and a top ten per
//! stage.

use std::cmp::Ordering;

use crate::menu::a2types::Reader;
use crate::menu::menu_types::{name32, SingleScore};
use crate::menu::Menu;
use crate::xvga::fileio::{file_delete, file_exist, file_read, file_write, save_path};

pub const SCORE_COUNT: usize = 10;
/// score_t: 0xc08 bytes.
const SCORE_DB_SIZE: usize = 0xc08;

fn write_score(out: &mut Vec<u8>, s: &SingleScore) {
    out.extend_from_slice(&s.name);
    out.extend_from_slice(&s.time.to_le_bytes());
    out.extend_from_slice(&s.stage.to_le_bytes());
    out.extend_from_slice(&s.is_new.to_le_bytes());
    out.extend_from_slice(&s.money.to_le_bytes());
}

fn read_score(r: &mut Reader) -> SingleScore {
    SingleScore { name: r.bytes::<32>(), time: r.i32(), stage: r.i16(), is_new: r.i16(), money: r.i32() }
}

fn get_last_entered(list: &[SingleScore]) -> i32 {
    list.iter().position(|s| s.is_new != 0).map(|i| i as i32).unwrap_or(-1)
}

/// Further stage first, then the shorter time, then by name.
fn compare_score(a: &SingleScore, b: &SingleScore) -> Ordering {
    b.stage.cmp(&a.stage).then(a.time.cmp(&b.time)).then(a.name.cmp(&b.name))
}

/// Adds an entry, sorts the eleven and keeps the best ten.
fn add_score(list: &mut [SingleScore; SCORE_COUNT], name: &[u8], time: i32, stage: i32, money: i32) {
    let mut buf: Vec<SingleScore> = list.iter().map(|s| SingleScore { is_new: 0, ..*s }).collect();
    buf.push(SingleScore { name: name32(name), time, stage: stage as i16, is_new: 1, money });
    buf.sort_by(compare_score);
    list.copy_from_slice(&buf[..SCORE_COUNT]);
}

impl Menu {
    fn scores_bytes(&self) -> Vec<u8> {
        let mut out = Vec::with_capacity(SCORE_DB_SIZE);
        for s in self.hiscore.iter().chain(self.levelscore.iter().flatten()) {
            write_score(&mut out, s);
        }
        out
    }

    /// The lists start out with the developers' names (score_list in A2MENU.DAT).
    fn default_scores(&mut self) {
        self.hiscore.copy_from_slice(&self.md.score_list[..SCORE_COUNT]);
        for (s, list) in self.levelscore.iter_mut().enumerate() {
            list.copy_from_slice(&self.md.score_list[SCORE_COUNT * (s + 1)..SCORE_COUNT * (s + 2)]);
        }
    }

    pub fn save_tmp_scoredbase(&self, name: &str) {
        if !file_write(&self.scores_bytes(), name) {
            eprintln!("save_scoredbase: cannot write {}", save_path(name).display());
        }
    }

    pub fn save_scoredbase(&self) {
        self.save_tmp_scoredbase("SCORE.A2R");
    }

    pub fn load_tmp_scoredbase(&mut self, name: &str) {
        let mut buf = vec![0u8; SCORE_DB_SIZE];
        if !file_read(&mut buf, name) {
            file_delete(name);
            self.init_scoredbase();
            return;
        }
        let mut r = Reader { p: &buf };
        for s in self.hiscore.iter_mut() {
            *s = read_score(&mut r);
        }
        for list in self.levelscore.iter_mut() {
            for s in list.iter_mut() {
                *s = read_score(&mut r);
            }
        }
    }

    pub fn init_scoredbase(&mut self) {
        if file_exist("SCORE.A2R") {
            self.load_tmp_scoredbase("SCORE.A2R");
        } else {
            self.default_scores();
            self.save_scoredbase();
        }
        // Nothing is new when the menu starts.
        for s in self.hiscore.iter_mut().chain(self.levelscore.iter_mut().flatten()) {
            s.is_new = 0;
        }
    }

    /// Rank of the entry added last, -1 when it did not make the list.
    pub fn get_last_entered_hiscore(&self) -> i32 {
        get_last_entered(&self.hiscore)
    }

    pub fn get_last_entered_levelscore(&self, stage: i32) -> i32 {
        get_last_entered(&self.levelscore[stage as usize])
    }

    pub fn add_new_hiscore(&mut self, name: &[u8], time: i32, stage: i32, money: i32) -> i32 {
        add_score(&mut self.hiscore, name, time, stage, money);
        self.get_last_entered_hiscore()
    }

    pub fn add_new_levelscore(&mut self, name: &[u8], time: i32, stage: i32, money: i32) -> i32 {
        // A stage list keeps 0 as the stage of every entry.
        add_score(&mut self.levelscore[stage as usize], name, time, 0, money);
        self.get_last_entered_levelscore(stage)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn score_file_has_the_original_size() {
        let m = Menu::new();
        assert_eq!(m.scores_bytes().len(), SCORE_DB_SIZE);
    }
}
