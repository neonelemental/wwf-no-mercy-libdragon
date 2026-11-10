//! Boots a ROM in mupen64plus, runs it for a number of frames and saves the
//! picture of the last one.
//!
//!     harness ROM FRAMES OUT.png
//!
//! A frame is one vertical interrupt. The emulator finds two of its parts in
//! this program by name: the input plugin, and the screen of the angrylion
//! video plugin, which the image builds without its OpenGL output.

#![allow(non_snake_case, non_upper_case_globals)]

use std::ffi::{c_char, c_int, c_uint, c_ulong, c_void, CStr, CString};
use std::path::PathBuf;
use std::process::exit;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::Mutex;
use std::time::Duration;
use std::{env, fs, process, ptr, thread};

const PLUGINS: &str = "/usr/local/lib/mupen64plus";
const DATA: &str = "/usr/local/share/mupen64plus";
/// Seconds without a frame before the run is given up.
const STALL: u64 = 60;

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
const MSG_WARNING: c_int = 2;

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

/// The run and the newest picture, as angrylion hands it over: RGBA rows of
/// `width`, to be shown `height_out` rows tall.
struct Run {
    target: u64,
    out: PathBuf,
    saved: bool,
    pixels: Vec<u8>,
    width: usize,
    height: usize,
    height_out: usize,
}

static RUN: Mutex<Option<Run>> = Mutex::new(None);
static FRAMES: AtomicU64 = AtomicU64::new(0);

fn fail(text: &str) -> ! {
    eprintln!("harness: {text}");
    exit(1)
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
    if level <= MSG_WARNING {
        eprintln!("mupen64plus: {}", unsafe { CStr::from_ptr(text) }.to_string_lossy());
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
    if frame != run.target {
        return;
    }
    if run.width == 0 || run.height_out == 0 {
        fail(&format!("no picture by frame {frame}"));
    }
    if let Err(error) = fs::write(&run.out, png(run)) {
        fail(&format!("{}: {error}", run.out.display()));
    }
    run.saved = true;
    drop(guard);
    CoreDoCommand(CMD_STOP, 0, ptr::null_mut());
}

// The input plugin (m64p_plugin.h): one standard controller with no pak, and
// no button held.

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
pub unsafe extern "C" fn GetKeys(_control: c_int, keys: *mut u32) {
    *keys = 0;
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
    let target = args.get(2).and_then(|frames| frames.parse::<u64>().ok()).filter(|&frames| frames > 0);
    let (Some(target), 4) = (target, args.len()) else {
        eprintln!("usage: harness ROM FRAMES OUT.png");
        exit(2)
    };
    let rom = fs::read(&args[1]).unwrap_or_else(|error| fail(&format!("{}: {error}", args[1])));
    *RUN.lock().unwrap() = Some(Run {
        target,
        out: PathBuf::from(&args[3]),
        saved: false,
        pixels: Vec::new(),
        width: 0,
        height: 0,
        height_out: 0,
    });

    // The core keeps its settings and the game's save here; each run starts with neither.
    let work = env::temp_dir().join(format!("harness-{}", process::id()));
    fs::create_dir_all(&work).unwrap_or_else(|error| fail(&format!("{}: {error}", work.display())));
    let work_c = cstring(&work.to_string_lossy());
    env::set_var("SDL_VIDEODRIVER", "dummy");
    env::set_var("SDL_AUDIODRIVER", "dummy");

    thread::spawn(|| {
        let mut last = 0;
        loop {
            thread::sleep(Duration::from_secs(STALL));
            let now = FRAMES.load(Ordering::Relaxed);
            if now == last {
                fail(&format!("no frame for {STALL} s after frame {now}"));
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
        set("Core", "SaveSRAMPath", TYPE_STRING, work_c.as_ptr() as *const c_void);
        check(CoreDoCommand(CMD_ROM_OPEN, rom.len() as c_int, rom.as_ptr() as *mut c_void), "opening the ROM");

        let core = open("libmupen64plus.so.2");
        attach(core, PLUGIN_GFX, "video-angrylion-plus");
        check(CoreAttachPlugin(PLUGIN_INPUT, dlopen(ptr::null(), RTLD_NOW)), "the input plugin");
        attach(core, PLUGIN_RSP, "rsp-cxd4");
        check(CoreDoCommand(CMD_EXECUTE, 0, ptr::null_mut()), "the run");
    }

    let _ = fs::remove_dir_all(&work);
    if !RUN.lock().unwrap().as_ref().unwrap().saved {
        fail(&format!("the emulator stopped at frame {}, before frame {target}", FRAMES.load(Ordering::Relaxed)));
    }
    println!("{} frames, picture in {}", target, args[3]);
}
