//! INI files (inifile.cpp). The original wrapped GetPrivateProfileInt and
//! WritePrivateProfileString on a file in the game directory; the port reads and
//! writes the same "[SECTION]" / "KEY=value" text itself, in the save directory.

use std::fs;
use std::path::PathBuf;
use std::sync::RwLock;

use crate::xvga::fileio::save_path;

static INI_FILENAME: RwLock<Option<PathBuf>> = RwLock::new(None);

pub fn set_ini_filename(name: &str) {
    if name.len() >= 0x10 {
        panic!("set_ini_filename: {name}: file name too long");
    }
    *INI_FILENAME.write().unwrap() = Some(save_path(name));
}

/// An INI file elsewhere (A2.INI on the disc).
pub fn set_ini_file_path(path: PathBuf) {
    *INI_FILENAME.write().unwrap() = Some(path);
}

fn ini_path(who: &str) -> PathBuf {
    INI_FILENAME.read().unwrap().clone().unwrap_or_else(|| panic!("{who}: no INI file set"))
}

/// Compares `name` with `text`, ignoring case and trailing blanks.
fn name_matches(text: &str, name: &str) -> bool {
    text.trim_end_matches([' ', '\t']).eq_ignore_ascii_case(name)
}

/// Where a key is: its line's byte range and its value's start, or where a new key goes
/// (the end of its section, or None when the section is missing).
enum Found {
    Key { line_start: usize, value_start: usize },
    Missing { insert: Option<usize> },
}

fn find_key(text: &str, section: &str, key: &str) -> Found {
    let mut in_section = false;
    let mut insert = None;
    let mut pos = 0;
    while pos < text.len() {
        let rest = &text[pos..];
        let eol = rest.find(['\r', '\n']).unwrap_or(rest.len());
        let mut next = eol;
        while rest[next..].starts_with(['\r', '\n']) {
            next += 1;
        }
        let line = &rest[..eol];
        let p = line.trim_start_matches([' ', '\t']);
        if let Some(body) = p.strip_prefix('[') {
            in_section = body.find(']').map(|c| name_matches(&body[..c], section)).unwrap_or(false);
        } else if in_section && !p.is_empty() {
            if let Some(eq) = p.find('=') {
                if name_matches(&p[..eq], key) {
                    let value_start = pos + (eol - p.len()) + eq + 1;
                    return Found::Key { line_start: pos, value_start };
                }
            }
        }
        if in_section {
            insert = Some(pos + next);
        }
        pos += next;
    }
    Found::Missing { insert }
}

pub fn write_ini_string_setting(section: &str, key: &str, value: &str) {
    let path = ini_path("write_ini_string_setting");
    let old = fs::read_to_string(&path).unwrap_or_default();
    let out = match find_key(&old, section, key) {
        Found::Key { line_start, value_start } => {
            // Replace the value, keep the rest.
            let eol = old[value_start..].find(['\r', '\n']).map(|e| value_start + e).unwrap_or(old.len());
            format!("{}{key}={value}{}", &old[..line_start], &old[eol..])
        }
        Found::Missing { insert: Some(at) } => {
            // A new key at the end of the section; the line before it may lack its end.
            let sep = if at > 0 && !old[..at].ends_with('\n') { "\r\n" } else { "" };
            format!("{}{sep}{key}={value}\r\n{}", &old[..at], &old[at..])
        }
        Found::Missing { insert: None } => {
            let sep = if !old.is_empty() && !old.ends_with('\n') { "\r\n" } else { "" };
            format!("{old}{sep}[{section}]\r\n{key}={value}\r\n")
        }
    };
    let _ = fs::write(&path, out);
}

pub fn write_ini_value_setting(section: &str, key: &str, value: i32) {
    write_ini_string_setting(section, key, &value.to_string());
}

pub fn read_ini_string_setting(section: &str, key: &str) -> String {
    let path = ini_path("read_ini_string_setting");
    let text = fs::read_to_string(&path).unwrap_or_default();
    match find_key(&text, section, key) {
        Found::Key { value_start, .. } => {
            let v = &text[value_start..];
            let eol = v.find(['\r', '\n']).unwrap_or(v.len());
            v[..eol].trim_matches([' ', '\t']).to_string()
        }
        Found::Missing { .. } => String::new(),
    }
}

/// 0 when the file, the section or the key is missing (GetPrivateProfileInt's default).
pub fn read_ini_value_setting(section: &str, key: &str) -> u32 {
    let s = read_ini_string_setting(section, key);
    // strtol: the leading number, 0 if none.
    let t = s.trim_start();
    let (neg, digits) = match t.strip_prefix('-') {
        Some(d) => (true, d),
        None => (false, t.strip_prefix('+').unwrap_or(t)),
    };
    let n: i64 = digits.chars().take_while(|c| c.is_ascii_digit()).collect::<String>().parse().unwrap_or(0);
    (if neg { -n } else { n }) as u32
}
