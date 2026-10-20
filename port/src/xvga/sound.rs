//! Sound buffers (sound.cpp). The original kept up to max_sound_buffers DirectSound
//! buffers, each holding one WAV by name; playing a name reuses its buffer, or a
//! duplicate when it is still playing. The port keeps that bookkeeping and mixes the
//! buffers itself into an SDL audio stream. Volumes are DirectSound's: hundredths of a
//! decibel, -10000 (silent) to 0.

use std::cell::RefCell;
use std::rc::Rc;
use std::sync::{Arc, Mutex, MutexGuard};

use sdl2::audio::{AudioCVT, AudioCallback, AudioDevice, AudioFormat, AudioSpecDesired, AudioSpecWAV};
use sdl2::rwops::RWops;

use crate::platform::sys::sdl;
use crate::res::archive::NamedArchive;

/// DSBPLAY_LOOPING
pub const SOUND_LOOPING: i32 = 1;
/// Keeps the buffer loaded (it is never the least used one).
pub const SOUND_KEEP: i32 = 4;

const MAX_BUFFERS: usize = 64;
const MAX_SAMPLES: usize = 64;

/// Decoded WAV data in the device's format, 16-bit stereo (shared like
/// DuplicateSoundBuffer does).
struct Sample {
    name: String,
    data: Arc<Vec<i16>>,
    /// The WAV's own rate, for set_sound_buffer_freq.
    rate: i32,
}

/// snd_buf_t in the original: name, DirectSound buffers, in-use flag, usage count.
#[derive(Clone, Default)]
struct SndBuf {
    name: String,
    /// The sample's data, None when the buffer is free.
    data: Option<Arc<Vec<i16>>>,
    rate: i32,
    /// In frames.
    pos: f64,
    /// Playback rate in Hz, 0 for the WAV's own.
    freq: i32,
    playing: bool,
    looping: bool,
    /// Hundredths of a dB.
    volume: i32,
    /// How often played; the least used buffer is freed first.
    use_count: i32,
}

struct Mixer {
    buffers: Vec<SndBuf>,
    max_sound_buffers: usize,
}

struct MixCallback(Arc<Mutex<Mixer>>);

impl AudioCallback for MixCallback {
    type Channel = i16;

    fn callback(&mut self, out: &mut [i16]) {
        out.fill(0);
        let mut m = self.0.lock().unwrap();
        let max = m.max_sound_buffers;
        for buf in m.buffers[..max].iter_mut() {
            if !buf.playing {
                continue;
            }
            let Some(data) = buf.data.clone() else { continue };
            // DirectSound volume: attenuation in hundredths of a decibel.
            let gain = if buf.volume <= -10000 { 0.0 } else { 10f32.powf(buf.volume as f32 / 2000.0) };
            // The data is at the device's rate; a set frequency plays it faster or slower.
            let step = if buf.freq > 0 && buf.rate > 0 { buf.freq as f64 / buf.rate as f64 } else { 1.0 };
            let count = data.len() / 2;
            for frame in out.chunks_exact_mut(2) {
                if buf.pos >= count as f64 {
                    if !buf.looping || count == 0 {
                        buf.playing = false;
                        break;
                    }
                    buf.pos -= count as f64;
                    if buf.pos >= count as f64 {
                        buf.pos = 0.0;
                    }
                }
                let a = buf.pos as usize;
                let b = if a + 1 < count { a + 1 } else if buf.looping { 0 } else { a };
                let t = (buf.pos - a as f64) as f32;
                for (c, o) in frame.iter_mut().enumerate() {
                    let (va, vb) = (data[a * 2 + c] as f32, data[b * 2 + c] as f32);
                    let x = va + (vb - va) * t;
                    *o = (*o as i32 + (x * gain) as i32).clamp(-32768, 32767) as i16;
                }
                buf.pos += step;
            }
        }
    }
}

struct Sound {
    _device: AudioDevice<MixCallback>,
    mixer: Arc<Mutex<Mixer>>,
    freq: i32,
    archive: Option<Rc<NamedArchive>>,
    samples: Vec<Sample>,
}

thread_local! {
    static SOUND: RefCell<Option<Sound>> = const { RefCell::new(None) };
}

fn with<R>(f: impl FnOnce(&mut Sound) -> R) -> Option<R> {
    SOUND.with(|s| s.borrow_mut().as_mut().map(f))
}

/// The sounds come from `files`. Ok on success (the original returned the sample rate
/// it got, or an error).
pub fn xvga_init_sound(max_buffers: usize, files: Option<Rc<NamedArchive>>) -> Result<(), String> {
    let audio = sdl().ok_or("no SDL")?.audio()?;
    let mixer = Arc::new(Mutex::new(Mixer {
        buffers: vec![SndBuf::default(); MAX_BUFFERS],
        max_sound_buffers: max_buffers.min(MAX_BUFFERS),
    }));
    // The original asked for 22050 Hz, 16-bit stereo first.
    let want = AudioSpecDesired { freq: Some(22050), channels: Some(2), samples: Some(1024) };
    let cb = mixer.clone();
    let device = audio.open_playback(None, &want, |_| MixCallback(cb))?;
    let freq = device.spec().freq;
    device.resume();
    SOUND.with(|s| *s.borrow_mut() = Some(Sound { _device: device, mixer, freq, archive: files, samples: Vec::new() }));
    Ok(())
}

pub fn xvga_exit_sound() {
    SOUND.with(|s| *s.borrow_mut() = None);
}

impl Sound {
    fn lock(&self) -> MutexGuard<'_, Mixer> {
        self.mixer.lock().unwrap()
    }

    fn convert(&self, data: Vec<u8>, format: AudioFormat, channels: u8, rate: i32) -> Option<Vec<i16>> {
        let cvt = AudioCVT::new(format, channels, rate, AudioFormat::s16_sys(), 2, self.freq).ok()?;
        let bytes = cvt.convert(data);
        Some(bytes.chunks_exact(2).map(|b| i16::from_ne_bytes([b[0], b[1]])).collect())
    }

    /// load_wave_file: the WAV from the archive, converted to the device format.
    fn load_sample(&mut self, filename: &str) -> Option<usize> {
        if let Some(i) = self.samples.iter().position(|s| s.name.eq_ignore_ascii_case(filename)) {
            return Some(i);
        }
        if self.samples.len() == MAX_SAMPLES {
            return None;
        }
        let Some(bytes) = self.archive.as_ref().and_then(|a| a.find(filename)) else {
            eprintln!("sound: {filename} not found");
            return None;
        };
        let wav = RWops::from_bytes(bytes).and_then(|mut rw| AudioSpecWAV::load_wav_rw(&mut rw));
        let wav = match wav {
            Ok(w) => w,
            Err(e) => {
                eprintln!("sound: {filename}: {e}");
                return None;
            }
        };
        let data = self.convert(wav.buffer().to_vec(), wav.format, wav.channels, wav.freq)?;
        self.samples.push(Sample { name: filename.to_string(), data: Arc::new(data), rate: wav.freq });
        Some(self.samples.len() - 1)
    }

    /// Finds or makes a buffer for the file (duplicating one still playing) and starts
    /// it; volume None keeps the buffer's.
    fn start_buffer(&mut self, filename: &str, flags: i32, volume: Option<i32>, play: bool) -> i32 {
        let sample = {
            let m = self.lock();
            let search = |start: usize, idle_only: bool| {
                (start..m.max_sound_buffers).find(|&i| {
                    let b = &m.buffers[i];
                    b.data.is_some() && b.name.eq_ignore_ascii_case(filename) && (!idle_only || !b.playing)
                })
            };
            let mut b = search(0, false);
            if let Some(i) = b {
                if m.buffers[i].playing {
                    // Still playing: use another idle copy of the same sound, or duplicate it.
                    b = search(i + 1, true);
                }
            }
            b
        };
        let b = match sample {
            Some(b) => b,
            None => {
                let Some(s) = self.load_sample(filename) else { return -1 };
                let (data, rate) = (self.samples[s].data.clone(), self.samples[s].rate);
                let mut m = self.lock();
                let max = m.max_sound_buffers;
                let free = (0..max).find(|&i| m.buffers[i].data.is_none());
                // delete_sound_buffer(-1): frees the least used idle buffer.
                let b = free.or_else(|| {
                    (0..max).filter(|&i| !m.buffers[i].playing).min_by_key(|&i| m.buffers[i].use_count)
                });
                let Some(b) = b else { return -1 };
                m.buffers[b] = SndBuf { name: filename.to_string(), data: Some(data), rate, ..Default::default() };
                b
            }
        };
        let mut m = self.lock();
        let buf = &mut m.buffers[b];
        buf.use_count = if flags & SOUND_KEEP != 0 { 1_000_000 } else { buf.use_count + 1 };
        if let Some(v) = volume {
            buf.volume = v;
        }
        buf.pos = 0.0;
        buf.looping = flags & SOUND_LOOPING != 0;
        buf.playing = play;
        b as i32
    }

    fn buffer(&self, index: i32) -> Option<usize> {
        (index >= 0 && (index as usize) < self.lock().max_sound_buffers).then_some(index as usize)
    }
}

/// Loads `filename` from the archive if needed and plays it; the buffer index or -1.
pub fn play_sound_buffer_1(filename: &str, flags: i32) -> i32 {
    with(|s| s.start_buffer(filename, flags, None, true)).unwrap_or(-1)
}

/// The same with a volume set before it starts.
pub fn play_sound_buffer_2(filename: &str, volume: i32, flags: i32) -> i32 {
    with(|s| s.start_buffer(filename, flags, Some(volume.clamp(-10000, 0)), true)).unwrap_or(-1)
}

/// A buffer for the file, not started; SOUND_KEEP keeps it loaded.
pub fn load_sound_buffer(filename: &str, flags: i32) -> i32 {
    with(|s| s.start_buffer(filename, flags, None, false)).unwrap_or(-1)
}

/// Starts a loaded buffer from the beginning.
pub fn play_sound_buffer(index: i32, flags: i32) {
    with(|s| {
        if let Some(i) = s.buffer(index) {
            let mut m = s.lock();
            let b = &mut m.buffers[i];
            if b.data.is_some() {
                b.pos = 0.0;
                b.looping = flags & SOUND_LOOPING != 0;
                b.playing = true;
            }
        }
    });
}

/// The playback rate in Hz (100..100000), DirectSound's SetFrequency.
pub fn set_sound_buffer_freq(index: i32, freq: i32) {
    if !(100..=100000).contains(&freq) {
        eprintln!("set_sound_buffer_freq: attempt at setting buffer {index} to {freq} Hz");
        return;
    }
    with(|s| {
        if let Some(i) = s.buffer(index) {
            s.lock().buffers[i].freq = freq;
        }
    });
}

/// Drops every buffer and sample and takes the sounds from another archive (the race
/// has its own process and sounds in the original).
pub fn xvga_set_sound_files(files: Option<Rc<NamedArchive>>, max_buffers: usize) {
    with(|s| {
        {
            let mut m = s.lock();
            for b in m.buffers.iter_mut() {
                *b = SndBuf::default();
            }
            m.max_sound_buffers = max_buffers.min(MAX_BUFFERS);
        }
        s.samples.clear();
        s.archive = files;
    });
}

pub fn sound_buffer_busy(index: i32) -> bool {
    with(|s| s.buffer(index).map(|i| {
        let m = s.lock();
        m.buffers[i].data.is_some() && m.buffers[i].playing
    }))
    .flatten()
    .unwrap_or(false)
}

pub fn stop_sound_buffer(index: i32) {
    with(|s| {
        if let Some(i) = s.buffer(index) {
            let mut m = s.lock();
            m.buffers[i].playing = false;
            m.buffers[i].pos = 0.0;
        }
    });
}

pub fn stop_all_sound_buffers() {
    with(|s| {
        for b in s.lock().buffers.iter_mut() {
            b.playing = false;
            b.pos = 0.0;
        }
    });
}

pub fn set_sound_buffer_vol(index: i32, volume: i32) {
    with(|s| {
        if let Some(i) = s.buffer(index) {
            s.lock().buffers[i].volume = volume.clamp(-10000, 0);
        }
    });
}

pub fn get_sound_buffer_vol(index: i32) -> i32 {
    with(|s| s.buffer(index).map(|i| s.lock().buffers[i].volume)).flatten().unwrap_or(-10000)
}

pub fn max_sound_buffer_count() -> usize {
    with(|s| s.lock().max_sound_buffers).unwrap_or(0)
}

/// PCM from elsewhere (the movies' sound), 16-bit; the buffer index or -1.
pub fn play_pcm(pcm: &[i16], rate: i32, channels: u8, volume: i32) -> i32 {
    if pcm.is_empty() {
        return -1;
    }
    with(|s| {
        let bytes: Vec<u8> = pcm.iter().flat_map(|v| v.to_ne_bytes()).collect();
        let Some(data) = s.convert(bytes, AudioFormat::s16_sys(), channels, rate) else { return -1 };
        let mut m = s.lock();
        let max = m.max_sound_buffers;
        let free = (0..max).find(|&i| m.buffers[i].data.is_none());
        let b = free.or_else(|| (0..max).filter(|&i| !m.buffers[i].playing).min_by_key(|&i| m.buffers[i].use_count));
        let Some(b) = b else { return -1 };
        m.buffers[b] = SndBuf {
            name: "<pcm>".into(),
            data: Some(Arc::new(data)),
            rate,
            volume,
            use_count: 1_000_000,
            playing: true,
            ..Default::default()
        };
        b as i32
    })
    .unwrap_or(-1)
}
