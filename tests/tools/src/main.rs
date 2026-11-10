//! Boots a ROM in mupen64plus and plays a script on it.
//!
//!     harness ROM SCRIPT OUTDIR [SAVES]
//!
//! The game starts on a blank cartridge save and the run throws its save
//! away. With SAVES, the save is kept in that directory: the game finds the
//! one that an earlier run left there, and leaves its own.
//!
//! A script holds one command on each line, and `#` starts a comment:
//!
//!     run 400          run 400 frames
//!     press start 6    hold Start for 6 frames, then let go; a+b holds both
//!     shot title       save the picture as OUTDIR/title.png
//!     read u8 game_mode        print the u8, u16 or u32 at a symbol
//!     expect u8 game_mode 1    fail the run when it holds another value
//!
//! The symbols are those of the ELF beside the ROM, ROM.elf for ROM.z64.
//!
//! The run fails when libdragon stops the game for a fault or a failed assert,
//! when the game reads or writes outside RDRAM through the cache, when the
//! game shows no new picture for 10 seconds or plays no sound for 30, and
//! when the script ends without both in its last second. It then prints
//! where the CPU is and saves the picture as OUTDIR/failed.png, which holds
//! libdragon's report when it stopped the game.
//!
//! A frame is one vertical interrupt. The emulator finds two of its parts in
//! this program by name: the input plugin, and the screen of the angrylion
//! video plugin, which the image builds without its OpenGL output.

#![allow(non_snake_case, non_upper_case_globals)]

use std::ffi::{c_char, c_int, c_uint, c_ulong, c_void, CStr, CString};
use std::path::{Path, PathBuf};
use std::process::exit;
use std::sync::atomic::{AtomicBool, AtomicU32, AtomicU64, Ordering};
use std::sync::Mutex;
use std::time::Duration;
use std::{env, fs, process, ptr, thread};

const PLUGINS: &str = "/usr/local/lib/mupen64plus";
const DATA: &str = "/usr/local/share/mupen64plus";
/// Seconds without a frame before the run is given up.
const STALL: u64 = 60;
/// Frames with no new picture, and with no sound, before the run fails. A
/// boot into a match goes at most 252 and 905 without.
const STILL: u64 = 600;
const HUSH: u64 = 1800;
/// A run must end with a picture and a sound newer than this.
const LIVE: u64 = 60;

// m64p_types.h
const CORE_API: c_int = 0x020001;
const CMD_ROM_OPEN: c_int = 1;
const CMD_EXECUTE: c_int = 5;
const CMD_STOP: c_int = 6;
const CMD_CORE_STATE_SET: c_int = 17;
const CORE_SPEED_LIMITER: c_int = 5;
const TYPE_INT: c_int = 1;
const TYPE_BOOL: c_int = 3;
const TYPE_STRING: c_int = 4;
const PLUGIN_RSP: c_int = 1;
const PLUGIN_GFX: c_int = 2;
const PLUGIN_INPUT: c_int = 4;
const MSG_ERROR: c_int = 1;
const PTR_RDRAM: c_int = 1;
const PTR_VI: c_int = 4;
const PTR_AI: c_int = 6;
const CPU_PC: c_int = 1;
const CPU_REGS: c_int = 2;
const CPU_COP0: c_int = 5;
const RDRAM_WORDS: usize = 0x80_0000 / 4;

type Message = extern "C" fn(*mut c_void, c_int, *const c_char);

#[link(name = "mupen64plus")]
extern "C" {
    fn CoreStartup(
        api: c_int,
        config: *const c_char,
        data: *const c_char,
        context: *mut c_void,
        message: Option<Message>,
        context2: *mut c_void,
        state: *const c_void,
    ) -> c_int;
    fn CoreDoCommand(command: c_int, int: c_int, ptr: *mut c_void) -> c_int;
    fn CoreAttachPlugin(kind: c_int, handle: *mut c_void) -> c_int;
    fn ConfigOpenSection(name: *const c_char, handle: *mut *mut c_void) -> c_int;
    fn ConfigSetParameter(handle: *mut c_void, name: *const c_char, kind: c_int, value: *const c_void) -> c_int;
    fn DebugMemGetPointer(kind: c_int) -> *mut c_void;
    fn DebugGetCPUDataPtr(kind: c_int) -> *mut c_void;
}

#[link(name = "z")]
extern "C" {
    fn compressBound(len: c_ulong) -> c_ulong;
    fn compress2(dest: *mut u8, dest_len: *mut c_ulong, src: *const u8, len: c_ulong, level: c_int) -> c_int;
    fn crc32(crc: c_ulong, buf: *const u8, len: c_uint) -> c_ulong;
}

extern "C" {
    fn dlopen(path: *const c_char, flags: c_int) -> *mut c_void;
    fn dlsym(handle: *mut c_void, name: *const c_char) -> *mut c_void;
}
const RTLD_NOW: c_int = 2;

/// The buttons of controller 1, by their bits in m64p_plugin.h's BUTTONS.
const BUTTONS: [&str; 14] =
    ["right", "left", "down", "up", "start", "z", "b", "a", "cright", "cleft", "cdown", "cup", "r", "l"];

/// How long ago, in frames, the game last gave the console something new.
#[derive(Default)]
struct Watch {
    last: u32,
    quiet: u64,
    longest: u64,
}

impl Watch {
    fn saw(&mut self, value: u32, new: bool) {
        self.last = value;
        self.quiet = if new { 0 } else { self.quiet + 1 };
        self.longest = self.longest.max(self.quiet);
    }
}

enum Step {
    Run { frames: u64, keys: u32 },
    Shot(String),
    Read { name: String, address: u32, bytes: u32, expect: Option<u32> },
}

/// The script, and the newest picture as angrylion hands it over: RGBA rows
/// of `width`, to be shown `height_out` rows tall.
struct Run {
    steps: Vec<Step>,
    /// The step that runs now, and the frames it has left.
    step: usize,
    left: u64,
    out: PathBuf,
    done: bool,
    pixels: Vec<u8>,
    width: usize,
    height: usize,
    height_out: usize,
    picture: Watch,
    sound: Watch,
    /// Where libdragon notes that it stopped the game, when the ELF names
    /// it, and the frame at which it did.
    inspector: Option<u32>,
    crashed: Option<u64>,
}

static RUN: Mutex<Option<Run>> = Mutex::new(None);
static FRAMES: AtomicU64 = AtomicU64::new(0);
static KEYS: AtomicU32 = AtomicU32::new(0);
/// The emulator saw an access that stops the CPU of a console.
static FROZEN: AtomicBool = AtomicBool::new(false);

fn fail(text: &str) -> ! {
    eprintln!("harness: {text}");
    exit(1)
}

/// Says where the CPU is, and what its 32 registers hold.
unsafe fn cpu() {
    let pc = *(DebugGetCPUDataPtr(CPU_PC) as *const u32);
    let cop0 = DebugGetCPUDataPtr(CPU_COP0) as *const u32;
    eprintln!(
        "harness: pc {pc:08x} status {:08x} cause {:08x} epc {:08x} badvaddr {:08x}",
        *cop0.add(12),
        *cop0.add(13),
        *cop0.add(14),
        *cop0.add(8)
    );
    let regs = DebugGetCPUDataPtr(CPU_REGS) as *const u64;
    for row in (0..32).step_by(4) {
        let four: Vec<String> = (row..row + 4).map(|reg| format!("{:08x}", *regs.add(reg) as u32)).collect();
        eprintln!("harness: r{row:02} {}", four.join(" "));
    }
}

fn check(result: c_int, what: &str) {
    if result != 0 {
        fail(&format!("{what} failed with m64p error {result}"));
    }
}

fn cstring(text: &str) -> CString {
    CString::new(text).unwrap_or_else(|_| fail("a path holds a NUL byte"))
}

extern "C" fn message(_context: *mut c_void, level: c_int, text: *const c_char) {
    if level <= MSG_ERROR {
        let text = unsafe { CStr::from_ptr(text) }.to_string_lossy();
        eprintln!("mupen64plus: {text}");
        if text.starts_with("cached access outside RDRAM") {
            FROZEN.store(true, Ordering::Relaxed);
        }
    }
}

unsafe fn set(section: &str, name: &str, kind: c_int, value: *const c_void) {
    let mut handle = ptr::null_mut();
    check(ConfigOpenSection(cstring(section).as_ptr(), &mut handle), "ConfigOpenSection");
    check(ConfigSetParameter(handle, cstring(name).as_ptr(), kind, value), name);
}

unsafe fn open(path: &str) -> *mut c_void {
    let handle = dlopen(cstring(path).as_ptr(), RTLD_NOW);
    if handle.is_null() {
        fail(&format!("cannot load {path}"));
    }
    handle
}

/// Starts a plugin of the image and hands it to the core.
unsafe fn attach(core: *mut c_void, kind: c_int, name: &str) {
    let plugin = open(&format!("{PLUGINS}/mupen64plus-{name}.so"));
    let startup = dlsym(plugin, cstring("PluginStartup").as_ptr());
    if startup.is_null() {
        fail(&format!("{name} has no PluginStartup"));
    }
    let startup: extern "C" fn(*mut c_void, *mut c_void, Option<Message>) -> c_int = std::mem::transmute(startup);
    check(startup(core, ptr::null_mut(), Some(message)), name);
    check(CoreAttachPlugin(kind, plugin), name);
}

/// The address of a symbol of a 32-bit big-endian ELF. The compiler puts a
/// number after the name of a static of a routine (`name.5`), and NAME finds
/// that one when no symbol has the name alone.
fn symbol(elf: &[u8], name: &str) -> Option<u32> {
    let mut numbered = None;
    let word = |at: usize| Some(u32::from_be_bytes(elf.get(at..at + 4)?.try_into().ok()?) as usize);
    let half = |at: usize| Some(u16::from_be_bytes(elf.get(at..at + 2)?.try_into().ok()?) as usize);
    let (sections, size, count) = (word(0x20)?, half(0x2e)?, half(0x30)?);
    for section in (0..count).map(|index| sections + index * size) {
        if word(section + 4)? != 2 {
            continue;
        }
        let (symbols, len) = (word(section + 0x10)?, word(section + 0x14)?);
        let strings = word(sections + word(section + 0x18)? * size + 0x10)?;
        for entry in (symbols..symbols + len).step_by(16) {
            let text = elf.get(strings + word(entry)?..)?;
            if text.starts_with(name.as_bytes()) {
                match text.get(name.len()) {
                    Some(0) => return Some(word(entry + 4)? as u32),
                    Some(b'.') => numbered = numbered.or(Some(word(entry + 4)? as u32)),
                    _ => {}
                }
            }
        }
    }
    numbered
}

/// The number that the game holds in `bytes` bytes at a KSEG0 address.
unsafe fn peek(address: u32, bytes: u32) -> Option<u32> {
    let rdram = DebugMemGetPointer(PTR_RDRAM) as *const u32;
    let from = address.checked_sub(0x8000_0000)? as usize;
    if from + bytes as usize > RDRAM_WORDS * 4 {
        return None;
    }
    Some((from..from + bytes as usize).fold(0, |value, at| value << 8 | *rdram.add(at / 4) >> (24 - 8 * (at % 4)) & 0xff))
}

fn parse(script: &str, elf: Option<&[u8]>) -> Result<Vec<Step>, String> {
    let value_of = |word: &str| match word.strip_prefix("0x") {
        Some(hex) => u32::from_str_radix(hex, 16).ok(),
        None => word.parse().ok(),
    };
    let read = |kind: &str, name: &str, expect: Option<u32>| {
        let bytes = ["u8", "u16", "", "u32"].iter().position(|&width| width == kind)? as u32 + 1;
        Some(Step::Read { name: name.to_string(), address: symbol(elf?, name)?, bytes, expect })
    };
    let mut steps = Vec::new();
    for (number, line) in script.lines().enumerate() {
        let words: Vec<&str> = line.split('#').next().unwrap_or("").split_whitespace().collect();
        let frames = |word: &str| word.parse::<u64>().ok().filter(|&frames| frames > 0);
        let step = match words[..] {
            [] => continue,
            ["run", count] => frames(count).map(|frames| Step::Run { frames, keys: 0 }),
            ["press", buttons, count] => frames(count).and_then(|frames| {
                let bits = buttons.split('+').map(|name| BUTTONS.iter().position(|&button| button == name));
                Some(Step::Run { frames, keys: bits.map(|bit| bit.map(|bit| 1 << bit)).sum::<Option<u32>>()? })
            }),
            ["shot", name] if !name.contains('/') => Some(Step::Shot(name.to_string())),
            ["read", kind, name] => read(kind, name, None),
            ["expect", kind, name, value] => value_of(value).and_then(|value| read(kind, name, Some(value))),
            _ => None,
        };
        steps.push(step.ok_or_else(|| format!("line {}: {}", number + 1, line.trim()))?);
    }
    Ok(steps)
}

/// The loudest sample of the buffer that the game last gave the AI.
unsafe fn loudest(buffer: u32, len: u32) -> u16 {
    let rdram = DebugMemGetPointer(PTR_RDRAM) as *const u32;
    let from = (buffer as usize / 4).min(RDRAM_WORDS);
    let words = std::slice::from_raw_parts(rdram.add(from), (len as usize / 4).min(RDRAM_WORDS - from));
    words.iter().flat_map(|word| [(word >> 16) as i16, *word as i16]).map(i16::unsigned_abs).max().unwrap_or(0)
}

impl Run {
    /// Fails the run: says why and where the CPU is, and saves the picture.
    unsafe fn stopped(&self, frame: u64, why: &str) -> ! {
        eprintln!("harness: {why}, at frame {frame}");
        cpu();
        if self.width > 0 && self.height_out > 0 {
            let _ = fs::write(self.out.join("failed.png"), png(self));
        }
        exit(1)
    }

    /// Notes whether this frame brought a new picture and a buffer of sound,
    /// and fails the run when either has been gone too long.
    /// The picture is new when the VI origin moves to another framebuffer,
    /// not by the line or two between the fields of one.
    unsafe fn watch(&mut self, frame: u64) {
        let origin = *(DebugMemGetPointer(PTR_VI) as *const u32).add(1);
        let ai = DebugMemGetPointer(PTR_AI) as *const u32;
        let (buffer, len) = (*ai & 0xff_ffff, *ai.add(1));
        self.picture.saw(origin, origin.abs_diff(self.picture.last) > 0x1000);
        self.sound.saw(buffer, buffer != self.sound.last && loudest(buffer, len) > 0);
        // libdragon draws its report of a fault or of a failed assert, which
        // takes it some seconds. The run goes on until that is the picture.
        if self.inspector.and_then(|at| peek(at, 1)).is_some_and(|on| on != 0) {
            let since = *self.crashed.get_or_insert(frame);
            let drawn = frame - self.picture.quiet > since && self.picture.quiet >= 2;
            if drawn || frame >= since + STILL {
                self.stopped(frame, "libdragon stopped the game, and the picture is its report");
            }
        }
        if FROZEN.load(Ordering::Relaxed) {
            self.stopped(frame, "a cached access outside RDRAM, which stops the CPU of a console");
        }
        if self.picture.quiet >= STILL {
            self.stopped(frame, &format!("no new picture for {STILL} frames"));
        }
        if self.sound.quiet >= HUSH {
            self.stopped(frame, &format!("no sound for {HUSH} frames"));
        }
    }

    /// Saves the pictures that the script asks for here, then holds the keys
    /// of the next run. With no run left, the script is done.
    unsafe fn next(&mut self, frame: u64) {
        while let Some(step) = self.steps.get(self.step) {
            match step {
                Step::Run { frames, keys } => {
                    self.left = *frames;
                    KEYS.store(*keys, Ordering::Relaxed);
                    return;
                }
                Step::Shot(name) => {
                    if self.width == 0 || self.height_out == 0 {
                        fail(&format!("no picture by frame {frame}"));
                    }
                    let path = self.out.join(format!("{name}.png"));
                    if let Err(error) = fs::write(&path, png(self)) {
                        fail(&format!("{}: {error}", path.display()));
                    }
                    println!("frame {frame}: {}", path.display());
                    self.step += 1;
                }
                Step::Read { name, address, bytes, expect } => {
                    if frame == 0 {
                        fail(&format!("{name} is read before the first run"));
                    }
                    let value = peek(*address, *bytes).unwrap_or_else(|| fail(&format!("{name} is not in RDRAM")));
                    println!("frame {frame}: {name} = {value:#x}");
                    if expect.is_some_and(|expect| expect != value) {
                        self.stopped(frame, &format!("{name} is {value:#x}, not {:#x}", expect.unwrap()));
                    }
                    self.step += 1;
                }
            }
        }
        KEYS.store(0, Ordering::Relaxed);
        self.done = true;
    }
}

fn chunk(png: &mut Vec<u8>, kind: &[u8; 4], data: &[u8]) {
    png.extend((data.len() as u32).to_be_bytes());
    let start = png.len();
    png.extend(kind);
    png.extend(data);
    let crc = unsafe { crc32(0, png[start..].as_ptr(), (png.len() - start) as c_uint) };
    png.extend((crc as u32).to_be_bytes());
}

/// The picture as a PNG, its rows repeated or dropped to reach `height_out`.
fn png(run: &Run) -> Vec<u8> {
    let mut raw = Vec::with_capacity(run.height_out * (1 + run.width * 3));
    for row in 0..run.height_out {
        let from = row * run.height / run.height_out * run.width * 4;
        raw.push(0);
        for pixel in run.pixels[from..from + run.width * 4].chunks_exact(4) {
            raw.extend(&pixel[..3]);
        }
    }
    let mut packed = vec![0u8; unsafe { compressBound(raw.len() as c_ulong) } as usize];
    let mut len = packed.len() as c_ulong;
    if unsafe { compress2(packed.as_mut_ptr(), &mut len, raw.as_ptr(), raw.len() as c_ulong, 6) } != 0 {
        fail("zlib could not pack the picture");
    }
    let mut header = Vec::new();
    header.extend((run.width as u32).to_be_bytes());
    header.extend((run.height_out as u32).to_be_bytes());
    header.extend([8, 2, 0, 0, 0]);
    let mut png = b"\x89PNG\r\n\x1a\n".to_vec();
    chunk(&mut png, b"IHDR", &header);
    chunk(&mut png, b"IDAT", &packed[..len as usize]);
    chunk(&mut png, b"IEND", &[]);
    png
}

// The screen of angrylion (src/output/vdac.h and screen.h).

#[repr(C)]
pub struct FrameBuffer {
    pixels: *const u8,
    width: u32,
    height: u32,
    height_out: u32,
    pitch: u32,
    valid: bool,
}

#[no_mangle]
pub static mut win_width: i32 = 0;
#[no_mangle]
pub static mut win_height: i32 = 0;
#[no_mangle]
pub static mut win_fullscreen: i32 = 0;

#[no_mangle]
pub extern "C" fn vdac_init(_config: *mut c_void) {}

#[no_mangle]
pub extern "C" fn vdac_close() {}

#[no_mangle]
pub extern "C" fn screen_toggle_fullscreen() {}

#[no_mangle]
pub unsafe extern "C" fn vdac_read(frame: *mut FrameBuffer, _alpha: bool) {
    (*frame).width = 0;
    (*frame).height = 0;
}

#[no_mangle]
pub unsafe extern "C" fn vdac_write(frame: *const FrameBuffer) {
    let frame = &*frame;
    let (width, height) = (frame.width as usize, frame.height as usize);
    let mut guard = RUN.lock().unwrap();
    let run = guard.as_mut().unwrap();
    run.pixels.clear();
    for row in 0..height {
        let from = frame.pixels.add(row * frame.pitch as usize * 4);
        run.pixels.extend_from_slice(std::slice::from_raw_parts(from, width * 4));
    }
    (run.width, run.height, run.height_out) = (width, height, frame.height_out as usize);
}

/// Called once for each vertical interrupt, after `vdac_write` when the frame
/// has a picture.
#[no_mangle]
pub unsafe extern "C" fn vdac_sync(_valid: bool) {
    let frame = FRAMES.fetch_add(1, Ordering::Relaxed) + 1;
    if frame == 1 {
        let mut off: c_int = 0;
        CoreDoCommand(CMD_CORE_STATE_SET, CORE_SPEED_LIMITER, &mut off as *mut c_int as *mut c_void);
    }
    let mut guard = RUN.lock().unwrap();
    let run = guard.as_mut().unwrap();
    if run.done {
        return;
    }
    run.watch(frame);
    run.left -= 1;
    if run.left > 0 {
        return;
    }
    run.step += 1;
    run.next(frame);
    let done = run.done;
    if done && run.picture.quiet > LIVE {
        run.stopped(frame, &format!("the script ends {} frames after the last new picture", run.picture.quiet));
    }
    if done && run.sound.quiet > LIVE {
        run.stopped(frame, &format!("the script ends {} frames after the last sound", run.sound.quiet));
    }
    drop(guard);
    if done {
        CoreDoCommand(CMD_STOP, 0, ptr::null_mut());
    }
}

// The input plugin (m64p_plugin.h): one standard controller with no pak,
// which holds the keys of the script.

#[repr(C)]
pub struct Control {
    present: c_int,
    raw_data: c_int,
    plugin: c_int,
    kind: c_int,
}

#[repr(C)]
pub struct ControlInfo {
    controls: *mut Control,
}

#[no_mangle]
pub unsafe extern "C" fn PluginGetVersion(
    kind: *mut c_int,
    version: *mut c_int,
    api: *mut c_int,
    name: *mut *const c_char,
    capabilities: *mut c_int,
) -> c_int {
    for (to, value) in [(kind, PLUGIN_INPUT), (version, 0x000100), (api, 0x020100), (capabilities, 0)] {
        if !to.is_null() {
            *to = value;
        }
    }
    if !name.is_null() {
        *name = b"harness input\0".as_ptr() as *const c_char;
    }
    0
}

#[no_mangle]
pub unsafe extern "C" fn InitiateControllers(info: ControlInfo) {
    for port in 0..4 {
        *info.controls.add(port) = Control { present: (port == 0) as c_int, raw_data: 0, plugin: 1, kind: 0 };
    }
}

#[no_mangle]
pub unsafe extern "C" fn GetKeys(control: c_int, keys: *mut u32) {
    *keys = if control == 0 { KEYS.load(Ordering::Relaxed) } else { 0 };
}

#[no_mangle]
pub extern "C" fn ControllerCommand(_control: c_int, _command: *mut u8) {}

#[no_mangle]
pub extern "C" fn ReadController(_control: c_int, _command: *mut u8) {}

#[no_mangle]
pub extern "C" fn RomOpen() -> c_int {
    1
}

#[no_mangle]
pub extern "C" fn RomClosed() {}

#[no_mangle]
pub extern "C" fn SDL_KeyDown(_modifiers: c_int, _key: c_int) {}

#[no_mangle]
pub extern "C" fn SDL_KeyUp(_modifiers: c_int, _key: c_int) {}

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() != 4 && args.len() != 5 {
        eprintln!("usage: harness ROM SCRIPT OUTDIR [SAVES]");
        exit(2)
    }
    let read = |path: &String| fs::read(path).unwrap_or_else(|error| fail(&format!("{path}: {error}")));
    let rom = read(&args[1]);
    let elf = fs::read(Path::new(&args[1]).with_extension("elf")).ok();
    let steps = parse(&String::from_utf8_lossy(&read(&args[2])), elf.as_deref())
        .unwrap_or_else(|line| fail(&format!("{}, {line}", args[2])));
    let out = PathBuf::from(&args[3]);
    fs::create_dir_all(&out).unwrap_or_else(|error| fail(&format!("{}: {error}", args[3])));
    let mut run = Run {
        steps,
        step: 0,
        left: 0,
        out,
        done: false,
        pixels: Vec::new(),
        width: 0,
        height: 0,
        height_out: 0,
        picture: Watch::default(),
        sound: Watch::default(),
        inspector: elf.as_deref().and_then(|elf| symbol(elf, "in_inspector")),
        crashed: None,
    };
    unsafe { run.next(0) };
    if run.done {
        fail(&format!("{} runs no frames", args[2]));
    }
    *RUN.lock().unwrap() = Some(run);

    // The core keeps its settings here, and the game's save when no SAVES is given.
    let work = env::temp_dir().join(format!("harness-{}", process::id()));
    fs::create_dir_all(&work).unwrap_or_else(|error| fail(&format!("{}: {error}", work.display())));
    let work_c = cstring(&work.to_string_lossy());
    let saves_c = cstring(args.get(4).map_or(&*work.to_string_lossy(), |saves| saves));
    env::set_var("SDL_VIDEODRIVER", "dummy");
    env::set_var("SDL_AUDIODRIVER", "dummy");

    thread::spawn(|| {
        let mut last = 0;
        loop {
            thread::sleep(Duration::from_secs(STALL));
            let now = FRAMES.load(Ordering::Relaxed);
            if now == last {
                eprintln!("harness: no frame for {STALL} s after frame {now}");
                unsafe { cpu() };
                exit(1)
            }
            last = now;
        }
    });

    unsafe {
        check(
            CoreStartup(
                CORE_API,
                work_c.as_ptr(),
                cstring(DATA).as_ptr(),
                ptr::null_mut(),
                Some(message),
                ptr::null_mut(),
                ptr::null(),
            ),
            "CoreStartup",
        );
        let (pure, no): (c_int, c_int) = (0, 0);
        set("Core", "R4300Emulator", TYPE_INT, &pure as *const c_int as *const c_void);
        set("Core", "RandomizeInterrupt", TYPE_BOOL, &no as *const c_int as *const c_void);
        set("Core", "SaveSRAMPath", TYPE_STRING, saves_c.as_ptr() as *const c_void);
        check(CoreDoCommand(CMD_ROM_OPEN, rom.len() as c_int, rom.as_ptr() as *mut c_void), "opening the ROM");

        let core = open("libmupen64plus.so.2");
        attach(core, PLUGIN_GFX, "video-angrylion-plus");
        check(CoreAttachPlugin(PLUGIN_INPUT, dlopen(ptr::null(), RTLD_NOW)), "the input plugin");
        attach(core, PLUGIN_RSP, "rsp-cxd4");
        check(CoreDoCommand(CMD_EXECUTE, 0, ptr::null_mut()), "the run");
    }

    let _ = fs::remove_dir_all(&work);
    let frames = FRAMES.load(Ordering::Relaxed);
    let guard = RUN.lock().unwrap();
    let run = guard.as_ref().unwrap();
    if !run.done {
        fail(&format!("the emulator stopped at frame {frames}, before the end of {}", args[2]));
    }
    println!(
        "{frames} frames, at most {} with no new picture and {} with no sound",
        run.picture.longest, run.sound.longest
    );
}
