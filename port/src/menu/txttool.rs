//! Text helpers of the menu (txttool.cpp).

use crate::xvga::font::{get_string_height, get_string_width, win_print_xy};

/// Prints text, shortened with "..." until it is narrower than max_width.
pub fn win_bprintf(which: usize, x: i32, y: i32, max_width: i32, text: &[u8]) {
    let mut text = text.to_vec();
    // Trailing blanks are dropped.
    while text.len() > 1 && b" \n\t".contains(text.last().unwrap()) {
        text.pop();
    }
    loop {
        let n = text.len();
        let w = get_string_width(&text, n);
        if w < max_width || w < 5 || n < 4 {
            win_print_xy(which, x, y, &text);
            return;
        }
        // One character shorter, ending in "...".
        text.truncate(n - 1);
        let m = text.len();
        text[m - 3..].copy_from_slice(b"...");
    }
}

fn is_blank(c: u8) -> bool {
    c == 9 || c == 10 || c == b' '
}

/// txt_get_word: blanks before a word plus the word.
fn txt_get_word(s: &[u8]) -> &[u8] {
    let mut seen = false;
    for (i, &c) in s.iter().enumerate() {
        if is_blank(c) {
            if seen {
                return &s[..i];
            }
        } else {
            seen = true;
        }
    }
    s
}

/// txt_check_newline: whether the blanks before the word contain a line break.
fn txt_check_newline(word: &[u8]) -> bool {
    word.iter().take_while(|&&c| is_blank(c)).any(|&c| c == 10 || c == 9)
}

fn txt_crop_word(word: &[u8]) -> &[u8] {
    let n = word.iter().take_while(|&&c| is_blank(c)).count();
    &word[n..]
}

/// Prints text word-wrapped between x and x + width, from y down.
pub fn win_printwrapped(which: usize, x: i32, mut y: i32, width: i32, text: &[u8]) {
    let right = x + width;
    let mut pos = 0;
    let mut cx = x;
    while pos < text.len() {
        let word = txt_get_word(&text[pos..]);
        pos += word.len().max(1);
        let mut out = word;
        let mut w = get_string_width(word, word.len());
        if w + cx >= right || txt_check_newline(word) {
            // Next line, without the blanks in front of the word.
            y += get_string_height(b"H", 1);
            out = txt_crop_word(word);
            w = get_string_width(out, out.len());
            cx = x;
        }
        win_print_xy(which, cx, y, out);
        cx += w;
    }
}
