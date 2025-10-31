// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT

//! Overlay application and top-level API
pub mod lua;

use crate::logging;
use crate::logging::{debug, info, warn, error};
use crate::utils;
use crate::dx;
use crate::settings;
use crate::ui;
use crate::input;
use crate::lua_manager;
use crate::ml;

use std::ffi::CString;
use std::sync::Arc;
use std::sync::Mutex;
use std::sync::atomic;

use std::time;

use windows::core::PCSTR;
use windows::Win32::Media;
use windows::Win32::System::Diagnostics::Debug;
use windows::Win32::Foundation;
use windows::Win32::UI::WindowsAndMessaging;
use windows::Win32::UI::Shell;
use windows::Win32::System::LibraryLoader;
use windows::Win32::Graphics::Gdi;
use windows::Win32::System::Threading;

const OVERLAY_WIN_CLASS: &str = "EG-Overlay Window";

static OVERLAY: Mutex<Option<Arc<EgOverlay>>> = Mutex::new(None);

const ICON_16X16: u64 = 0x01;

const WM_SYSTRAYEVENT  : u32 = WindowsAndMessaging::WM_APP + 1;
const WM_SYSTRAYQUIT   : u32 = WindowsAndMessaging::WM_APP + 2;
const WM_SYSTRAYLOG    : u32 = WindowsAndMessaging::WM_APP + 3;
const WM_SYSTRAYDOCS   : u32 = WindowsAndMessaging::WM_APP + 4;
const WM_SYSTRAYRESTART: u32 = WindowsAndMessaging::WM_APP + 5;

/// The global state for the entire overlay
pub struct EgOverlay {
    hwnd: atomic::AtomicUsize,
    target_hwnd: atomic::AtomicUsize,
    target_win_class: String,

    tray_menu: atomic::AtomicUsize,

    running: atomic::AtomicBool,
    visible: atomic::AtomicBool,

    frame_count: atomic::AtomicU64,

    mods: Mutex<OverlayModules>,

    settings: Arc<settings::SettingsStore>,

    input: Arc<input::InputManager>,

    start_time: time::Instant,

    restart: atomic::AtomicBool,

    do_resize: atomic::AtomicBool,
}

/// The state for various internal (Rust) modules.
///
/// This is separate from the rest of the state so that the entire set of modules
/// can be stored into a single mutex.
struct OverlayModules {
    dx: Option<Arc<dx::Dx>>,
    ui: Option<Arc<ui::Ui>>,
    ml: Option<Arc<ml::MumbleLink>>,
}

/// Outputs panic information to the overlay logs.
///
/// If a debugger is attached this will also trigger a breakpoint after logging.
fn log_panic(info: &std::panic::PanicHookInfo) {
    let location = info.location().unwrap();

    if let Some(payload) = info.payload().downcast_ref::<&str>() {
        error!("Panic at {}:{}: {}", location.file(), location.line(), payload);
    } else if let Some(payload) = info.payload().downcast_ref::<String>() {
        error!("Panic at {}:{}: {}", location.file(), location.line(), payload);
    } else {
        error!("Panic at {}:{}", location.file(), location.line());
    }

    unsafe {
    if Debug::IsDebuggerPresent().into() {
            debug!("Removing input hooks before panic break:");
            overlay().input.remove_hooks();
            Debug::DebugBreak();
        }
    }
}

pub fn init() {
    let start_time = time::Instant::now();

    logging::init(logging::LoggingLevel::Info);

    if utils::have_console() {
        // log to the console if we have one
        logging::add_sink(Box::new(logging::sinks::ConsoleSink::new()));
    } else {
        // otherwise see if there's a debugger attached and log there instead
        unsafe { if windows::Win32::System::Diagnostics::Debug::IsDebuggerPresent().into() {
            logging::add_sink(Box::new(logging::sinks::DbgSink::new()));
        }}
    }

    // in any case, log to file
    logging::add_sink(Box::new(logging::sinks::FileSink::new("eg-overlay.log")));

    // and log to Lua
    logging::add_sink(lua_manager::LuaLogSink::new());

    info!("============================================================");
    info!("EG-Overlay startup");
    info!("Version   : {}", crate::version::VERSION_STR);
    info!("Git Commit: {}", crate::githash::GITHASH_STR);
    info!("------------------------------------------------------------");

    let mut wincls = String::from("ArenaNet_Gr_Window_Class");

    let mut args = std::env::args();

    let _ = args.next(); // skip program path
    while let Some(a) = args.next() {
        match a.as_str() {
            "" => {},
            "--debug" => {
                logging::set_default_level(logging::LoggingLevel::Debug);
                debug!("Debug logging enabled.");
            },
            "--target-win-class" => {
                if let Some(wc) = args.next() {
                    wincls = wc.clone();
                    warn!("Target window class set to {}", wincls);
                } else {
                    panic!("--target-win-class requires a string argument.");
                }
            },
            _ => error!("Unrecognized command line argument: {}", a),
        }
    }

    std::panic::set_hook(Box::new(log_panic));

    let overlay_settings = settings::SettingsStore::new("eg-overlay");
    overlay_settings.set_default_value("overlay.frameTargetTime",  32.0);
    overlay_settings.set_default_value("overlay.luaUpdateTarget",  32.0);
    overlay_settings.set_default_value("overlay.fgWinCheckTime" , 250.0);

    let overlay = EgOverlay {
        hwnd: atomic::AtomicUsize::new(0),
        target_hwnd: atomic::AtomicUsize::new(0),
        target_win_class: wincls,

        tray_menu: atomic::AtomicUsize::new(0),
        running: atomic::AtomicBool::new(false),
        visible: atomic::AtomicBool::new(false),

        frame_count: atomic::AtomicU64::new(0),

        settings: overlay_settings,
        start_time: start_time,

        input: input::InputManager::new(),

        mods: Mutex::new(OverlayModules {
            dx: None,
            ui: None,
            ml: None,
        }),

        restart: atomic::AtomicBool::new(false),
        do_resize: atomic::AtomicBool::new(false),
    };

    *OVERLAY.lock().unwrap() = Some(Arc::new(overlay));

    let o = crate::overlay::overlay();

    utils::init_com_for_thread();

    crate::lua_sqlite3::init();

    register_win_class();
    create_window();
    create_tray_menu();

    // Lua has to be brought up before nearly everything else, so that modules
    // can register openers, etc.
    lua_manager::init();

    crate::lua_shell::init();
    crate::lua_path::init();

    // don't keep mods locked, Ui::new needs dx, etc.
    o.mods.lock().unwrap().dx = Some(dx::Dx::new());
    o.mods.lock().unwrap().ui = Some(ui::Ui::new());
    o.mods.lock().unwrap().ml = Some(ml::MumbleLink::new());

    // input needs a reference to UI now that it's up
    input::set_ui(&ui());

    crate::web_request::init();
}

fn register_win_class() {
    let mut cls = WindowsAndMessaging::WNDCLASSEXA::default();
    let clsnm = CString::new(OVERLAY_WIN_CLASS).unwrap();

    cls.cbSize = std::mem::size_of::<WindowsAndMessaging::WNDCLASSEXA>() as u32;
    cls.lpfnWndProc = Some(overlay_wnd_proc);
    cls.style = WindowsAndMessaging::CS_HREDRAW | WindowsAndMessaging::CS_VREDRAW;
    cls.lpszClassName = PCSTR(clsnm.as_bytes().as_ptr());

    let r = unsafe { WindowsAndMessaging::RegisterClassExA(&cls) };
    if r==0 {
        panic!("Failed to register window class.");
    }
}

fn unregister_win_class() {
    let clsnm = CString::new(OVERLAY_WIN_CLASS).unwrap();
    let r = unsafe { WindowsAndMessaging::UnregisterClassA(PCSTR(clsnm.as_bytes().as_ptr()), None) };

    r.expect("Failed to unregister window class.");
}

fn create_window() {
    let clsnm = CString::new(OVERLAY_WIN_CLASS).unwrap();
    let winnm = CString::new("EG-Overlay").unwrap();

    let exstyle = WindowsAndMessaging::WS_EX_NOREDIRECTIONBITMAP |
                  WindowsAndMessaging::WS_EX_TOOLWINDOW          |
                  WindowsAndMessaging::WS_EX_TRANSPARENT         |
                  WindowsAndMessaging::WS_EX_LAYERED             |
                  WindowsAndMessaging::WS_EX_NOACTIVATE;
    let style   = WindowsAndMessaging::WS_POPUP        |
                  WindowsAndMessaging::WS_CLIPSIBLINGS |
                  WindowsAndMessaging::WS_CLIPCHILDREN;

    let r = unsafe { WindowsAndMessaging::CreateWindowExA(
        exstyle,
        PCSTR(clsnm.as_bytes().as_ptr()),
        PCSTR(winnm.as_bytes().as_ptr()),
        style,
        10,
        10,
        1280,
        720,
        None,
        None,
        None,
        None
    ) };

    if r.is_err() {
        panic!("Couldn't create window");
    }

    overlay().hwnd.store(r.unwrap().0 as usize, atomic::Ordering::Relaxed);
}

fn create_tray_menu() {
    use WindowsAndMessaging::AppendMenuA;
    use WindowsAndMessaging::{MF_GRAYED, MF_STRING, MF_SEPARATOR, MF_ENABLED};
    use windows::core::s;

    let menu = unsafe { WindowsAndMessaging::CreatePopupMenu().unwrap() };

    unsafe {
        AppendMenuA(menu, MF_GRAYED | MF_STRING , 0                         , s!("EG-Overlay")        ).unwrap();
        AppendMenuA(menu, MF_SEPARATOR          , 0                         , PCSTR::null()           ).unwrap();
        AppendMenuA(menu, MF_ENABLED | MF_STRING, WM_SYSTRAYDOCS as usize   , s!("Open documentation")).unwrap();
        AppendMenuA(menu, MF_ENABLED | MF_STRING, WM_SYSTRAYLOG  as usize   , s!("Open log file")     ).unwrap();
        AppendMenuA(menu, MF_SEPARATOR          , 0                         , PCSTR::null()           ).unwrap();
        AppendMenuA(menu, MF_ENABLED | MF_STRING, WM_SYSTRAYRESTART as usize, s!("Restart")           ).unwrap();
        AppendMenuA(menu, MF_ENABLED | MF_STRING, WM_SYSTRAYQUIT as usize   , s!("Quit")              ).unwrap();
    }

    overlay().tray_menu.store(menu.0 as usize, atomic::Ordering::Relaxed);
}

fn show_tray_menu(x: i32, y: i32) -> i32 {
    let o = overlay();
    let menu = WindowsAndMessaging::HMENU(o.tray_menu.load(atomic::Ordering::Relaxed) as *mut std::ffi::c_void);

    let cmd = unsafe { WindowsAndMessaging::TrackPopupMenu(
        menu,
        WindowsAndMessaging::TPM_RETURNCMD,
        x,
        y,
        None,
        o.hwnd(),
        None
    )};

    return cmd.0;
}

pub fn run() {
    let overlay = crate::overlay::overlay();

    let mut nid = Shell::NOTIFYICONDATAA::default();
    nid.cbSize = std::mem::size_of::<Shell::NOTIFYICONDATAA>() as u32;
    nid.hWnd = overlay.hwnd();
    nid.uID = 0x01;
    nid.uFlags = Shell::NIF_ICON | Shell::NIF_MESSAGE | Shell::NIF_TIP | Shell::NIF_SHOWTIP;
    nid.uCallbackMessage = WM_SYSTRAYEVENT;
    nid.Anonymous.uVersion = Shell::NOTIFYICON_VERSION_4;
    nid.hIcon = unsafe {
        WindowsAndMessaging::LoadIconA(
            Some(LibraryLoader::GetModuleHandleA(None).unwrap().into()),
            windows::core::PCSTR(ICON_16X16 as *const u8)
        ).unwrap()
    };
    let overlay_tip = c"EG-Overlay";
    unsafe {
        std::ptr::copy_nonoverlapping(
            overlay_tip.as_ptr() as *const i8,
            nid.szTip.as_mut_ptr(),
            overlay_tip.to_bytes().len()
        );
        Shell::Shell_NotifyIconA(Shell::NIM_ADD, &nid).expect("Failed to set shell tray icon.");
        Shell::Shell_NotifyIconA(Shell::NIM_SETVERSION, &nid).expect("Failed to set shell icon version.");
    }

    lua_manager::add_module_opener("overlay", Some(crate::overlay::lua::open_module));

    overlay.running.store(true, atomic::Ordering::Relaxed);

    debug!("Starting render thread...");

    let overlay_render = overlay.clone();
    let render = std::thread::Builder::new().name("EG-Overlay Render Thread".to_string()).spawn(move || {
        render_thread(overlay_render);
    }).expect("Couldn't spawn render thread.");

    lua_manager::start_thread();

    let mut last_fg_check = 0.0f64;

    let fg_win_check_time: f64 = overlay.settings.get_f64("overlay.fgWinCheckTime").unwrap();

    // the window that was FG last time we checked.
    let mut last_win = Foundation::HWND(0 as *mut std::ffi::c_void);

    let overlay_hwnd = overlay.hwnd();

    let mut msg = WindowsAndMessaging::MSG::default();
    while msg.message != WindowsAndMessaging::WM_QUIT {
        unsafe {
            if WindowsAndMessaging::PeekMessageA(&mut msg as *mut _, None, 0, 0, WindowsAndMessaging::PM_REMOVE).into() {
                let _ = WindowsAndMessaging::TranslateMessage(&msg as *const _);
                WindowsAndMessaging::DispatchMessageA(&msg as *const _);
            }
        }

        let now = uptime().as_secs_f64();

        if (now - last_fg_check) * 1000.0 >= fg_win_check_time {
            let fg_win = unsafe { WindowsAndMessaging::GetForegroundWindow() };

            if fg_win!=last_win {
                // foreground window has changed since we last looked
                let mut cls_name_bytes = [0u8;512];

                if unsafe { WindowsAndMessaging::GetClassNameA(fg_win, &mut cls_name_bytes) } == 0 {
                    //warn!("Couldn't get foreground window class name.");
                    last_fg_check = now;
                    continue;
                }

                let cls_name = std::ffi::CStr::from_bytes_until_nul(&cls_name_bytes).unwrap().to_str().unwrap();

                if cls_name == overlay.target_win_class && !overlay.visible.load(atomic::Ordering::Relaxed) {
                    debug!("Target window active, showing overlay. {}", cls_name);

                    unsafe { let _ = WindowsAndMessaging::ShowWindow(overlay_hwnd, WindowsAndMessaging::SW_SHOWNA); }

                    overlay.target_hwnd.store(fg_win.0 as usize, atomic::Ordering::Relaxed);
                    overlay.visible.store(true, atomic::Ordering::Relaxed);
                }

                if fg_win.0 as usize == overlay.target_hwnd.load(atomic::Ordering::Relaxed) {
                    overlay.input.install_hooks();

                    let mut target_rect = Foundation::RECT::default();
                    unsafe { WindowsAndMessaging::GetClientRect(fg_win, &mut target_rect).unwrap() };

                    let mut target_pos = Foundation::POINT {
                        x: target_rect.left,
                        y: target_rect.top,
                    };

                    unsafe { Gdi::ClientToScreen(fg_win, &mut target_pos).unwrap() };

                    let x = target_pos.x;
                    let y = target_pos.y;
                    let w = target_rect.right - target_rect.left;
                    let h = target_rect.bottom - target_rect.top;

                    unsafe {
                        _ =WindowsAndMessaging::SetWindowPos(
                            overlay_hwnd,
                            Some(WindowsAndMessaging::HWND_TOPMOST),
                            0, 0,
                            0, 0,
                            WindowsAndMessaging::SWP_NOACTIVATE | WindowsAndMessaging::SWP_NOMOVE | WindowsAndMessaging::SWP_NOSIZE
                        );
                        _ = WindowsAndMessaging::SetWindowPos(
                            overlay_hwnd,
                            Some(WindowsAndMessaging::HWND_NOTOPMOST),
                            x,y,
                            w,h,
                            WindowsAndMessaging::SWP_NOACTIVATE
                        );
                    }

                } else {
                    overlay.input.remove_hooks();
                    unsafe {
                        _ = WindowsAndMessaging::SetWindowPos(
                            overlay_hwnd,
                            Some(WindowsAndMessaging::HWND_NOTOPMOST),
                            0, 0,
                            0, 0,
                            WindowsAndMessaging::SWP_NOACTIVATE | WindowsAndMessaging::SWP_NOMOVE | WindowsAndMessaging::SWP_NOSIZE
                        );
                    }
                }
            }

            let target_hwnd = Foundation::HWND(overlay.target_hwnd.load(atomic::Ordering::Relaxed) as *mut std::ffi::c_void);
            let mut target_cls_bytes = [0u8; 512];

            if overlay.target_hwnd.load(atomic::Ordering::Relaxed) != 0 &&
               ( unsafe { WindowsAndMessaging::GetClassNameA(target_hwnd, &mut target_cls_bytes) } == 0 ||
                 std::ffi::CStr::from_bytes_until_nul(&target_cls_bytes).unwrap().to_str().unwrap()!=overlay.target_win_class
               ) {
                debug!("Target window disappeared, hiding overlay.");
                unsafe { let _ = WindowsAndMessaging::ShowWindow(overlay_hwnd, WindowsAndMessaging::SW_HIDE); };
                overlay.target_hwnd.store(0, atomic::Ordering::Relaxed);
                overlay.input.remove_hooks();
                overlay.visible.store(false, atomic::Ordering::Relaxed);
            } /*else if fg_win == target_hwnd {
                let mut target_rect = Foundation::RECT::default();
                unsafe { WindowsAndMessaging::GetClientRect(target_hwnd, &mut target_rect).unwrap() };

                let mut target_pos = Foundation::POINT {
                    x: target_rect.left,
                    y: target_rect.top,
                };

                unsafe { Gdi::ClientToScreen(target_hwnd, &mut target_pos).unwrap() };

                let x = target_pos.x;
                let y = target_pos.y;
                let w = target_rect.right - target_rect.left;
                let h = target_rect.bottom - target_rect.top;

                unsafe {
                    WindowsAndMessaging::SetWindowPos(
                            overlay.hwnd(),
                            Some(WindowsAndMessaging::HWND_TOPMOST),
                            0, 0,
                            0, 0,
                            WindowsAndMessaging::SWP_NOACTIVATE | WindowsAndMessaging::SWP_NOMOVE | WindowsAndMessaging::SWP_NOSIZE
                        ).unwrap();
                        WindowsAndMessaging::SetWindowPos(
                            overlay.hwnd(),
                            Some(WindowsAndMessaging::HWND_NOTOPMOST),
                            x,y,
                            w,h,
                            WindowsAndMessaging::SWP_NOACTIVATE
                        ).unwrap();
                }
            }*/

            last_win = fg_win;

            last_fg_check = now;
        }

        std::thread::sleep(std::time::Duration::from_millis(1));
    }

    overlay.input.remove_hooks();

    overlay.running.store(false, atomic::Ordering::Relaxed);

    debug!("Waiting for threads to end...");
    lua_manager::stop_thread();
    render.join().expect("Render thread panicked.");
}

pub fn overlay() -> Arc<EgOverlay> {
    OVERLAY.lock().unwrap().as_ref().unwrap().clone()
}

impl EgOverlay {
    pub fn hwnd(&self) -> Foundation::HWND {
        return Foundation::HWND(self.hwnd.load(atomic::Ordering::Relaxed) as *mut std::ffi::c_void);
    }

    pub fn uptime(&self) -> time::Duration {
        let now = time::Instant::now();
        return now - self.start_time;
    }

    pub fn settings(&self) -> Arc<settings::SettingsStore> {
        return self.settings.clone();
    }

    pub fn dx(&self) -> Arc<dx::Dx> {
        self.mods.lock().unwrap().dx.as_ref().unwrap().clone()
    }

    pub fn ui(&self) -> Arc<ui::Ui> {
        self.mods.lock().unwrap().ui.as_ref().unwrap().clone()
    }


    pub fn ml(&self) -> Arc<ml::MumbleLink> {
        self.mods.lock().unwrap().ml.as_ref().unwrap().clone()
    }

    /*

    pub fn input(&self) -> Arc<input::InputManager> {
        self.input.clone()
    }
    */
}

pub fn restart() {
    OVERLAY.lock().unwrap().as_mut().unwrap().restart.store(true, atomic::Ordering::SeqCst);

    exit();
}

pub fn exit() {
    let hwnd = OVERLAY.lock().unwrap().as_ref().unwrap().hwnd();
    unsafe {
        WindowsAndMessaging::PostMessageA(
            Some(hwnd),
            WindowsAndMessaging::WM_CLOSE,
            Foundation::WPARAM(0),
            Foundation::LPARAM(0)
        ).unwrap();
    }
}

pub fn cleanup() {
    crate::web_request::cleanup();

    lua_manager::cleanup();

    let do_restart = OVERLAY.lock().unwrap().as_ref().unwrap().restart.load(atomic::Ordering::SeqCst);

    *OVERLAY.lock().unwrap() = None;

    if unsafe { Debug::IsDebuggerPresent().into() } {
        dx::report_live_objects();
    }

    unregister_win_class();

    crate::lua_sqlite3::cleanup();

    utils::uninit_com_for_thread();

    let _ = std::panic::take_hook();

    info!("------------------------------------------------------------");
    info!("EG-Overlay shutdown");
    info!("============================================================");

    crate::logging::cleanup();

    if do_restart {
        let cmd = unsafe { windows::Win32::System::Environment::GetCommandLineA().to_string().unwrap() };

        let cmdcstr = std::ffi::CString::new(cmd).unwrap();
        let cmdraw = cmdcstr.into_raw();

        let mut si = Threading::STARTUPINFOA::default();
        si.cb = std::mem::size_of::<Threading::STARTUPINFOA>() as u32;

        let mut pi = Threading::PROCESS_INFORMATION::default();

        unsafe {
            Threading::CreateProcessA(
                None,
                Some(windows::core::PSTR::from_raw(cmdraw as *mut _)),
                None,
                None,
                false.into(),
                Threading::PROCESS_CREATION_FLAGS(0),
                None,
                None,
                &si,
                &mut pi
            ).unwrap();
        }

        let _ = unsafe { std::ffi::CString::from_raw(cmdraw) };
    }
}

unsafe extern "system" fn overlay_wnd_proc(
    hwnd: Foundation::HWND,
    msg: u32,
    wparam: Foundation::WPARAM,
    lparam: Foundation::LPARAM
) -> Foundation::LRESULT {
    match msg {
        WindowsAndMessaging::WM_CLOSE => unsafe {
            WindowsAndMessaging::DestroyWindow(hwnd).unwrap();
        },
        WindowsAndMessaging::WM_DESTROY => unsafe {
            WindowsAndMessaging::PostQuitMessage(0);
        },
        WindowsAndMessaging::WM_SIZE => {
            let overlay = OVERLAY.lock().unwrap();
            if overlay.is_none() { return Foundation::LRESULT(0); }

            let o = overlay.as_ref().unwrap();

            o.do_resize.store(true, atomic::Ordering::Relaxed);
        },
        WM_SYSTRAYEVENT => {
            if (lparam.0 & 0xffff) as u32 == WindowsAndMessaging::WM_CONTEXTMENU {
                let x = (wparam.0 & 0xffff) as i32;
                let y = ((wparam.0 >> 16) & 0xffff) as i32;

                // if the window isn't foreground the menu will not close if the user clicks out of it
                // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-trackpopupmenu
                unsafe { WindowsAndMessaging::SetForegroundWindow(hwnd) }.unwrap();
                let cmd = show_tray_menu(x, y);

                match cmd as u32 {
                    0 => {}, // user closed menu with no selection
                    WM_SYSTRAYQUIT => {
                        debug!("Quit selected.");
                        exit();
                    },
                    WM_SYSTRAYLOG => {
                        debug!("Opening log file...");
                        unsafe { Shell::ShellExecuteA(
                            None,
                            windows::core::s!("open"),
                            windows::core::s!("eg-overlay.log"),
                            None,
                            None,
                            WindowsAndMessaging::SW_SHOWNORMAL
                        )};
                    },
                    WM_SYSTRAYDOCS => {
                        debug!("Opening documentation...");
                        unsafe { Shell::ShellExecuteA(
                            None,
                            windows::core::s!("open"),
                            windows::core::s!("https://the-eg.github.io/EG-Overlay"),
                            None,
                            None,
                            WindowsAndMessaging::SW_SHOWNORMAL
                        )};
                    },
                    WM_SYSTRAYRESTART => {
                        restart();
                    }
                    _ => { error!("Unknown system tray menu command: {}", cmd); },
                }
            }
        },
        _ => unsafe { return WindowsAndMessaging::DefWindowProcA(hwnd, msg, wparam, lparam); }
    }

    return Foundation::LRESULT(0);
}

fn render_thread(overlay: Arc<EgOverlay>) {
    debug!("Begin render thread.");

    utils::init_com_for_thread();

    // Set the timer resolution to the minimum possible, hopefully 1ms.
    // This allows Sleep to be much more accurate, which will help us maintain
    // a steady FPS with lower CPU usage.
    let mut tc = Media::TIMECAPS::default();
    if unsafe { Media::timeGetDevCaps(&mut tc, std::mem::size_of::<Media::TIMECAPS>() as u32) } != Media::MMSYSERR_NOERROR {
        panic!("timeGetDevCaps error.");
    }

    debug!("Setting timer resolution to {} ms.", tc.wPeriodMin);

    if unsafe { Media::timeBeginPeriod(tc.wPeriodMin) } != Media::MMSYSERR_NOERROR {
        error!( "Couldn't set timer resolution.");
    }

    let frame_target = overlay.settings.get_f64("overlay.frameTargetTime").unwrap();

    debug!("Frame target time: {}ms (~{:.0} FPS).", frame_target, 1000.0 / frame_target);

    let ui = ui();

    let odx = overlay.dx();

    dx::lua::init(&odx, &overlay.ml(), &ui);

    while overlay.running.load(atomic::Ordering::Relaxed) {
        if overlay.visible.load(atomic::Ordering::Relaxed) {
            if overlay.do_resize.load(atomic::Ordering::Relaxed) {
                odx.resize_swapchain(overlay.hwnd());
                overlay.do_resize.store(false, atomic::Ordering::Relaxed);
            }

            let frame_begin = overlay.uptime().as_secs_f64();

            if let Some(mut frame) = odx.start_frame() {
                dx::lua::render(&mut frame);
                ui.draw(&mut frame);
                frame.end_frame();

                overlay.frame_count.fetch_add(1, atomic::Ordering::Relaxed);
            }

            let frame_end = overlay.uptime().as_secs_f64();
            let frame_time = (frame_end - frame_begin) * 1000.0;
            let sleep_time = frame_target - frame_time;

            // if we have extra time, sleep
            if sleep_time > 0.0 {
                std::thread::sleep(std::time::Duration::from_secs_f64(sleep_time / 1000.0));
            }
        } else {
            std::thread::sleep(std::time::Duration::from_millis(25));
        }
    }

    unsafe {
        Media::timeEndPeriod(tc.wPeriodMin);
    }

    utils::uninit_com_for_thread();

    dx::lua::cleanup();

    debug!("End render thread.");
}

pub fn dx() -> Arc<dx::Dx> {
    OVERLAY.lock().unwrap().as_ref().unwrap().dx()
}

pub fn ui() -> Arc<ui::Ui> {
    OVERLAY.lock().unwrap().as_ref().unwrap().ui()
}

/*
pub fn ml() -> Arc<ml::MumbleLink> {
    OVERLAY.lock().unwrap().as_ref().unwrap().ml()
}
*/

pub fn settings() -> Arc<settings::SettingsStore> {
    OVERLAY.lock().unwrap().as_ref().unwrap().settings()
}

pub fn hwnd() -> Foundation::HWND {
    OVERLAY.lock().unwrap().as_ref().unwrap().hwnd()
}

pub fn uptime() -> time::Duration {
    OVERLAY.lock().unwrap().as_ref().unwrap().uptime()
}

/*
pub fn input() -> Arc<input::InputManager> {
    OVERLAY.lock().unwrap().as_ref().unwrap().input()
}
*/

pub fn frame_count() -> u64 {
    OVERLAY.lock().unwrap().as_ref().unwrap().frame_count.load(atomic::Ordering::Relaxed)
}

