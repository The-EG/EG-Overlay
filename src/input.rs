// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT

//! Mouse and keyboard input processing
#[allow(unused_imports)]
use crate::logging::{debug, info, warn, error};

use std::sync::Arc;
use std::sync::atomic;
use std::sync::Mutex;
use std::sync::Weak;

use windows::Win32::Foundation;
use windows::Win32::UI::WindowsAndMessaging;
use windows::Win32::UI::Input::KeyboardAndMouse;
use windows::Win32::Graphics::Gdi;

/// Global input state.
///
/// Currently just handles to the input hooks.
pub struct InputManager {
    mouse: HHookWrapper,
    keyboard: HHookWrapper,
}

/// A wrapper to store handles to input hooks
struct HHookWrapper(atomic::AtomicUsize);

/// Some flags to track mouse state.
///
/// These need to be accessed quickly since they will be in the input hook which
/// gets called on EVERY mouse event.
struct MouseState {
    // Store if a drag event was started in the target (ie GW2). If so, we don't want
    // process mouse events until the drag event ends (the mouse button is released).
    mouse_ldrag_target: bool,
    mouse_rdrag_target: bool,

    ui: Weak<crate::ui::Ui>,
}

/// Flags to track keyboard state
struct KeyboardState {
    ui: Weak<crate::ui::Ui>,
}


static MOUSE_STATE: Mutex<MouseState> = Mutex::new(MouseState {
    mouse_ldrag_target: false,
    mouse_rdrag_target: false,

    ui: Weak::new(),
});

static KEYBOARD_STATE: Mutex<KeyboardState> = Mutex::new(KeyboardState {
    ui: Weak::new(),
});

/// Stores a weak reference to UI for use by mouse and keyboard hooks.
pub fn set_ui(ui: &Arc<crate::ui::Ui>) {
    MOUSE_STATE.lock().unwrap().ui = Arc::downgrade(&ui);
    KEYBOARD_STATE.lock().unwrap().ui = Arc::downgrade(&ui);
}

impl InputManager {

    /// Initialize the Input Manager.
    pub fn new() -> Arc<InputManager> {
        let mut keyboard_layout = [0u8; 9];

        // for reference when I come back to implement different layouts:
        // https://learn.microsoft.com/en-us/windows-hardware/manufacture/desktop/windows-language-pack-default-values?view=windows-11#keyboard-identifiers
        unsafe {
            KeyboardAndMouse::GetKeyboardLayoutNameA(&mut keyboard_layout)
                .expect("Couldn't get keyboard layout nam");
        }

        let kl_str = String::from_utf8_lossy(&keyboard_layout[0..8]);

        if kl_str != "00000409" {
            error!("Input assumes US keyboard layout (00000409), {} found. Input keys will not match", kl_str);
        } else {
            debug!("Using keyboard layout {}", kl_str);
        }

        Arc::new(InputManager {
            mouse: HHookWrapper::new(),
            keyboard: HHookWrapper::new(),
        })
    }

    /// Install keyboard and mouse hooks
    pub fn install_hooks(&self) {
        debug!("Installing input hooks...");

        let mouse = unsafe { WindowsAndMessaging::SetWindowsHookExA(
            WindowsAndMessaging::WH_MOUSE_LL,
            Some(mouse_hook_proc),
            None,
            0
        ) }.expect("Couldn't set mouse input hook.");

        let keyboard = unsafe { WindowsAndMessaging::SetWindowsHookExA(
            WindowsAndMessaging::WH_KEYBOARD_LL,
            Some(keyboard_hook_proc),
            None,
            0
        ) }.expect("Couldn't set keyboard input hook.");

        self.mouse.store(mouse);
        self.keyboard.store(keyboard);
    }

    /// Remove keyboard and mouse hooks
    pub fn remove_hooks(&self) {
        if !self.mouse.is_empty() {
            debug!("Removing input hooks...");
            unsafe { WindowsAndMessaging::UnhookWindowsHookEx(self.mouse.load()) }
                .expect("Couldn't remove mouse input hook.");
            unsafe { WindowsAndMessaging::UnhookWindowsHookEx(self.keyboard.load()) }
                .expect("Couldn't remove keyboard input hook.");
            self.mouse.clear();
            self.keyboard.clear();
        }
    }
}

impl HHookWrapper {
    pub fn new() -> HHookWrapper {
        HHookWrapper(atomic::AtomicUsize::new(0))
    }

    pub fn store(&self, value: WindowsAndMessaging::HHOOK) {
        self.0.store(value.0 as usize, atomic::Ordering::Relaxed);
    }

    pub fn load(&self) -> WindowsAndMessaging::HHOOK {
        WindowsAndMessaging::HHOOK(self.0.load(atomic::Ordering::Relaxed) as *mut std::ffi::c_void)
    }

    pub fn clear(&self) {
        self.0.store(0, atomic::Ordering::Relaxed);
    }

    pub fn is_empty(&self) -> bool {
        self.0.load(atomic::Ordering::Relaxed) == 0
    }
}

#[derive(PartialEq)]
pub enum MouseButtonEventButton {
    Left,
    Right,
    Middle,
    X1,
    X2,
    Unknown,
}

impl std::fmt::Display for MouseButtonEventButton {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            MouseButtonEventButton::Left => f.pad("Left Button"),
            MouseButtonEventButton::Right => f.pad("Right Button"),
            MouseButtonEventButton::Middle => f.pad("Middle Button"),
            MouseButtonEventButton::X1 => f.pad("Button X1"),
            MouseButtonEventButton::X2 => f.pad("Button X2"),
            MouseButtonEventButton::Unknown => f.pad("Unknown Button"),
        }
    }
}

impl MouseButtonEventButton {
    pub fn from(msg: u32, msll: &WindowsAndMessaging::MSLLHOOKSTRUCT) -> MouseButtonEventButton {
        match msg {
            WindowsAndMessaging::WM_LBUTTONUP | WindowsAndMessaging::WM_LBUTTONDOWN => {
                return MouseButtonEventButton::Left;
            },
            WindowsAndMessaging::WM_RBUTTONUP | WindowsAndMessaging::WM_RBUTTONDOWN => {
                return MouseButtonEventButton::Right;
            },
            WindowsAndMessaging::WM_MBUTTONUP | WindowsAndMessaging::WM_MBUTTONDOWN => {
                return MouseButtonEventButton::Middle;
            },
            WindowsAndMessaging::WM_XBUTTONUP | WindowsAndMessaging::WM_XBUTTONDOWN => {
                match ((msll.mouseData >> 16) & 0xFFFF) as u16 {
                    WindowsAndMessaging::XBUTTON1 => { return MouseButtonEventButton::X1; },
                    WindowsAndMessaging::XBUTTON2 => { return MouseButtonEventButton::X2; },
                    _ => { }
                }
            },
            _ => { }
        }

        MouseButtonEventButton::Unknown
    }
}

pub struct MouseButtonEvent {
    pub x: i64,
    pub y: i64,
    pub button: MouseButtonEventButton,
    pub down: bool,
}

impl std::fmt::Display for MouseButtonEvent {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        if self.down {
            f.pad(format!("<Mouse {} down @ {},{}>", self.button, self.x, self.y).as_str())
        } else {
            f.pad(format!("<Mouse {} up @ {},{}>", self.button, self.x, self.y).as_str())
        }
    }
}

#[derive(Clone)]
pub struct MouseGenericEvent {
    pub x: i64,
    pub y: i64,
}

pub struct MouseWheelEvent {
    pub x: i64,
    pub y: i64,
    pub value: i32,
    pub horizontal: bool,
}

impl std::fmt::Display for MouseWheelEvent {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        if self.horizontal {
            f.pad(format!("<Mouse Horiztonal Wheel {} @ {},{}>", self.value, self.x, self.y).as_str())
        } else {
            f.pad(format!("<Mouse Vertical Wheel {} @ {},{}>", self.value, self.x, self.y).as_str())
        }
    }
}

pub enum MouseEvent {
    Move(MouseGenericEvent),
    Button(MouseButtonEvent),
    Enter(MouseGenericEvent),
    Leave(MouseGenericEvent),
    Wheel(MouseWheelEvent),
}

impl std::fmt::Display for MouseEvent {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            MouseEvent::Move(m) => f.pad(format!("<Mouse Move @ {},{}>", m.x, m.y).as_str()),
            MouseEvent::Button(b) => f.pad(format!("{}", b).as_str()),
            MouseEvent::Wheel(w) => f.pad(format!("{}", w).as_str()),
            MouseEvent::Enter(e) => f.pad(format!("<Mouse Enter @ {},{}>", e.x, e.y).as_str()),
            MouseEvent::Leave(l) => f.pad(format!("<Mouse Leave @ {},{}>", l.x, l.y).as_str()),
        }
    }
}

impl MouseEvent {
    pub fn x(&self) -> i64 {
        match self {
            MouseEvent::Move(m)   => m.x,
            MouseEvent::Button(b) => b.x,
            MouseEvent::Wheel(w)  => w.x,
            MouseEvent::Enter(e)  => e.x,
            MouseEvent::Leave(l)  => l.x,
        }
    }

    pub fn y(&self) -> i64 {
        match self {
            MouseEvent::Move(m)   => m.y,
            MouseEvent::Button(b) => b.y,
            MouseEvent::Wheel(w)  => w.y,
            MouseEvent::Enter(e)  => e.y,
            MouseEvent::Leave(l)  => l.y,
        }
    }


    pub fn from(msg: u32, msll: &WindowsAndMessaging::MSLLHOOKSTRUCT) -> MouseEvent {
        let mut p = Foundation::POINT {
            x: msll.pt.x,
            y: msll.pt.y,
        };

        let hwnd = crate::overlay::hwnd();

        unsafe { let _ = Gdi::ScreenToClient(hwnd, &mut p); };

        match msg {
            WindowsAndMessaging::WM_LBUTTONDOWN | WindowsAndMessaging::WM_LBUTTONUP |
            WindowsAndMessaging::WM_RBUTTONDOWN | WindowsAndMessaging::WM_RBUTTONUP |
            WindowsAndMessaging::WM_MBUTTONDOWN | WindowsAndMessaging::WM_MBUTTONUP |
            WindowsAndMessaging::WM_XBUTTONDOWN | WindowsAndMessaging::WM_XBUTTONUP => {
                let btn = MouseButtonEventButton::from(msg, msll);

                let down: bool = match msg {
                    WindowsAndMessaging::WM_LBUTTONDOWN |
                    WindowsAndMessaging::WM_RBUTTONDOWN |
                    WindowsAndMessaging::WM_MBUTTONDOWN |
                    WindowsAndMessaging::WM_XBUTTONDOWN => true,
                    _ => false,
                };

                return MouseEvent::Button( MouseButtonEvent {
                    x: p.x as i64,
                    y: p.y as i64,
                    button: btn,
                    down: down,
                });
            },
            WindowsAndMessaging::WM_MOUSEMOVE => {
                return MouseEvent::Move( MouseGenericEvent {
                    x: p.x as i64,
                    y: p.y as i64,
                });
            },
            WindowsAndMessaging::WM_MOUSEWHEEL | WindowsAndMessaging::WM_MOUSEHWHEEL => {
                let horizontal: bool = match msg {
                    WindowsAndMessaging::WM_MOUSEHWHEEL => true,
                    _ => false,
                };

                return MouseEvent::Wheel( MouseWheelEvent {
                    x: p.x as i64,
                    y: p.y as i64,
                    horizontal: horizontal,
                    value: ((msll.mouseData >> 16) & 0xFFFF) as i16 as i32 / WindowsAndMessaging::WHEEL_DELTA as i32,
                });
            },
            _ => { panic!("Unkown mouse input type."); }
        }
    }

    pub fn as_enter(&self) -> MouseEvent {
        match self {
            MouseEvent::Move(m) => MouseEvent::Enter(m.clone()),
            MouseEvent::Button(b) => MouseEvent::Enter(MouseGenericEvent { x: b.x, y: b.y }),
            MouseEvent::Wheel(w) => MouseEvent::Enter(MouseGenericEvent { x: w.x, y: w.y }),
            _ => panic!("Can only create enter event from a move, button, or wheel event."),
        }
    }

    pub fn as_leave(&self) -> MouseEvent {
        match self {
            MouseEvent::Move(m) => MouseEvent::Leave(m.clone()),
            MouseEvent::Button(b) => MouseEvent::Leave(MouseGenericEvent { x: b.x, y: b.y }),
            MouseEvent::Wheel(w) => MouseEvent::Leave(MouseGenericEvent { x: w.x, y: w.y }),
            _ => panic!("Can only create leave event from a move, button, or wheel event."),
        }
    }
}

unsafe extern "system" fn mouse_hook_proc(
    ncode: i32,
    wparam: Foundation::WPARAM,
    lparam: Foundation::LPARAM
) -> Foundation::LRESULT {
    if ncode < 0 {
        return unsafe { WindowsAndMessaging::CallNextHookEx(
            None,
            ncode,
            wparam,
            lparam
        ) };
    }

    let mut state = MOUSE_STATE.lock().unwrap();

    if wparam.0 as u32 == WindowsAndMessaging::WM_LBUTTONUP {
        if state.mouse_ldrag_target {
            state.mouse_ldrag_target = false;
            drop(state);
            return unsafe { WindowsAndMessaging::CallNextHookEx(None, ncode, wparam, lparam) }
        }
    }
    if wparam.0 as u32 == WindowsAndMessaging::WM_RBUTTONUP {
        if state.mouse_rdrag_target {
            state.mouse_rdrag_target = false;
            drop(state);
            return unsafe { WindowsAndMessaging::CallNextHookEx(None, ncode, wparam, lparam) }
        }
    }


    if state.mouse_ldrag_target || state.mouse_rdrag_target {
        drop(state);
        return unsafe { WindowsAndMessaging::CallNextHookEx(None, ncode, wparam, lparam) };
    }

    let event = MouseEvent::from(
        wparam.0 as u32,
        unsafe { &*(lparam.0 as *const WindowsAndMessaging::MSLLHOOKSTRUCT) }
    );

    let ui = state.ui.upgrade().unwrap();

    if ui.process_mouse_event(&event) {
        match wparam.0 as u32 {
            //WindowsAndMessaging::WM_LBUTTONUP |
            //WindowsAndMessaging::WM_RBUTTONUP |
            WindowsAndMessaging::WM_MOUSEMOVE => { return unsafe {
                WindowsAndMessaging::CallNextHookEx(None, ncode, wparam, lparam)
            }; },
            _ => { return Foundation::LRESULT(1); }
        }
    } else {
        // if this was a mouse button down event and we didn't consume it
        // then we don't want to process mouse events until the corresponding up
        // event comes through, otherwise we could be getting events during a
        // drag/mouse look.
        // this has to be tracked for all buttons separately (currently just
        // left and right)
        match wparam.0 as u32 {
            WindowsAndMessaging::WM_LBUTTONDOWN => {
                state.mouse_ldrag_target = true;
            },
            WindowsAndMessaging::WM_RBUTTONDOWN => {
                state.mouse_rdrag_target = true;
            },
            _ => {}
        }
    }

    // somehow we get called twice for the same message, causing a deadlock
    // for the state. drop it first here.
    drop(state);

    return unsafe { WindowsAndMessaging::CallNextHookEx(
        None,
        ncode,
        wparam,
        lparam
    ) };
}

pub struct KeyboardEvent {
    pub vkey: KeyboardAndMouse::VIRTUAL_KEY,
    pub down: bool,
    pub alt: bool,
    pub shift: bool,
    pub caps_lock: bool,
    pub ctrl: bool,
    pub chars: Option<String>,
}

impl KeyboardEvent {
    pub fn from(kbll: &WindowsAndMessaging::KBDLLHOOKSTRUCT) -> KeyboardEvent {
        let caps_on = unsafe { KeyboardAndMouse::GetKeyState(KeyboardAndMouse::VK_CAPITAL.0 as i32) & 0x01 == 1 };
        let shift   = unsafe { KeyboardAndMouse::GetKeyState(KeyboardAndMouse::VK_SHIFT.0   as i32) & 0x80 != 0 };
        let ctrl    = unsafe { KeyboardAndMouse::GetKeyState(KeyboardAndMouse::VK_CONTROL.0 as i32) & 0x80 != 0 };
        let alt     = unsafe { KeyboardAndMouse::GetKeyState(KeyboardAndMouse::VK_MENU.0    as i32) & 0x80 != 0 };

        let down = (kbll.flags & WindowsAndMessaging::LLKHF_UP)!=WindowsAndMessaging::LLKHF_UP;

        let vk = KeyboardAndMouse::VIRTUAL_KEY(kbll.vkCode as u16);

        KeyboardEvent {
            vkey: vk,
            down: down,
            alt: alt,
            shift: shift,
            caps_lock: caps_on,
            ctrl: ctrl,
            chars: vkey_to_string(vk, down, shift, alt, ctrl, caps_on),
        }
    }

    pub fn full_name(&self) -> String {
        format!(
            "{}{}{}{}",
            if self.ctrl  { "ctrl-"  } else { "" },
            if self.alt   { "alt-"   } else { "" },
            if self.shift { "shift-" } else { "" },
            vkey_name(self.vkey),
        )
    }

    pub fn to_string(&self) -> String {
        format!(
            "{}-{}",
            self.full_name(),
            if self.down { "down" } else { "up" },
        )
    }
}

impl std::fmt::Display for KeyboardEvent {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let nm = self.to_string();

        let chars = if let Some(c) = self.chars.as_ref() {
            format!(": \"{}\"", c)
        } else {
            String::new()
        };

        f.pad(&format!(
            "<Keyboard Event {} (caps: {}){}>",
            nm,
            if self.caps_lock { "on" } else { "off" },
            chars
        ))
    }
}

unsafe extern "system" fn keyboard_hook_proc(
    ncode: i32,
    wparam: Foundation::WPARAM,
    lparam: Foundation::LPARAM
) -> Foundation::LRESULT {
    if ncode < 0 {
        return unsafe { WindowsAndMessaging::CallNextHookEx(
            None,
            ncode,
            wparam,
            lparam
        ) };
    }

    let event = KeyboardEvent::from(unsafe { &*(lparam.0 as *const WindowsAndMessaging::KBDLLHOOKSTRUCT) });

    if KEYBOARD_STATE.lock().unwrap().ui.upgrade().unwrap().process_keyboard_event(&event) {
        // don't consume certain keystrokes, the system won't process them if we do
        if event.vkey != KeyboardAndMouse::VK_LMENU    &&
           event.vkey != KeyboardAndMouse::VK_RMENU    &&
           event.vkey != KeyboardAndMouse::VK_LCONTROL &&
           event.vkey != KeyboardAndMouse::VK_RCONTROL &&
           event.vkey != KeyboardAndMouse::VK_LSHIFT   &&
           event.vkey != KeyboardAndMouse::VK_RSHIFT   &&
           event.vkey != KeyboardAndMouse::VK_CAPITAL  &&
           event.vkey != KeyboardAndMouse::VK_NUMLOCK  &&
           event.vkey != KeyboardAndMouse::VK_SCROLL
        {
            return Foundation::LRESULT(1);
        }
    }

    crate::lua_manager::queue_event(&event.to_string(), None);

    return unsafe { WindowsAndMessaging::CallNextHookEx(
        None,
        ncode,
        wparam,
        lparam
    ) };
}

fn vkey_name(vkey: KeyboardAndMouse::VIRTUAL_KEY) -> String {
    match vkey {
        KeyboardAndMouse::VK_BACK       => String::from("backspace"),
        KeyboardAndMouse::VK_TAB        => String::from("tab"),
        KeyboardAndMouse::VK_CLEAR      => String::from("clear"),
        KeyboardAndMouse::VK_RETURN     => String::from("return"),
        KeyboardAndMouse::VK_SHIFT      => String::from("shift"),
        KeyboardAndMouse::VK_CONTROL    => String::from("ctrl"),
        KeyboardAndMouse::VK_MENU       => String::from("alt"),
        KeyboardAndMouse::VK_PAUSE      => String::from("pause"),
        KeyboardAndMouse::VK_CAPITAL    => String::from("capslock"),
        KeyboardAndMouse::VK_ESCAPE     => String::from("escape"),
        KeyboardAndMouse::VK_SPACE      => String::from("space"),
        KeyboardAndMouse::VK_PRIOR      => String::from("pageup"),
        KeyboardAndMouse::VK_NEXT       => String::from("pagedown"),
        KeyboardAndMouse::VK_END        => String::from("end"),
        KeyboardAndMouse::VK_HOME       => String::from("home"),
        KeyboardAndMouse::VK_LEFT       => String::from("left"),
        KeyboardAndMouse::VK_UP         => String::from("up"),
        KeyboardAndMouse::VK_RIGHT      => String::from("right"),
        KeyboardAndMouse::VK_DOWN       => String::from("down"),
        KeyboardAndMouse::VK_SELECT     => String::from("select"),
        KeyboardAndMouse::VK_PRINT      => String::from("print"),
        KeyboardAndMouse::VK_EXECUTE    => String::from("execute"),
        KeyboardAndMouse::VK_SNAPSHOT   => String::from("printscreen"),
        KeyboardAndMouse::VK_INSERT     => String::from("insert"),
        KeyboardAndMouse::VK_DELETE     => String::from("delete"),
        KeyboardAndMouse::VK_HELP       => String::from("help"),
        KeyboardAndMouse::VK_LWIN       => String::from("lwindows"),
        KeyboardAndMouse::VK_RWIN       => String::from("rwindows"),
        KeyboardAndMouse::VK_APPS       => String::from("applications"),
        KeyboardAndMouse::VK_ADD        => String::from("plus"),
        KeyboardAndMouse::VK_SEPARATOR  => String::from("separator"),
        KeyboardAndMouse::VK_NUMPAD0    => String::from("numpad0"),
        KeyboardAndMouse::VK_NUMPAD1    => String::from("numpad1"),
        KeyboardAndMouse::VK_NUMPAD2    => String::from("numpad2"),
        KeyboardAndMouse::VK_NUMPAD3    => String::from("numpad3"),
        KeyboardAndMouse::VK_NUMPAD4    => String::from("numpad4"),
        KeyboardAndMouse::VK_NUMPAD5    => String::from("numpad5"),
        KeyboardAndMouse::VK_NUMPAD6    => String::from("numpad6"),
        KeyboardAndMouse::VK_NUMPAD7    => String::from("numpad7"),
        KeyboardAndMouse::VK_NUMPAD8    => String::from("numpad8"),
        KeyboardAndMouse::VK_NUMPAD9    => String::from("numpad9"),
        KeyboardAndMouse::VK_MULTIPLY   => String::from("multiply"),
        KeyboardAndMouse::VK_SUBTRACT   => String::from("subtract"),
        KeyboardAndMouse::VK_DECIMAL    => String::from("decimal"),
        KeyboardAndMouse::VK_DIVIDE     => String::from("divide"),
        KeyboardAndMouse::VK_F1         => String::from("f1"),
        KeyboardAndMouse::VK_F2         => String::from("f2"),
        KeyboardAndMouse::VK_F3         => String::from("f3"),
        KeyboardAndMouse::VK_F4         => String::from("f4"),
        KeyboardAndMouse::VK_F5         => String::from("f5"),
        KeyboardAndMouse::VK_F6         => String::from("f6"),
        KeyboardAndMouse::VK_F7         => String::from("f7"),
        KeyboardAndMouse::VK_F8         => String::from("f8"),
        KeyboardAndMouse::VK_F9         => String::from("f9"),
        KeyboardAndMouse::VK_F10        => String::from("f10"),
        KeyboardAndMouse::VK_F11        => String::from("f11"),
        KeyboardAndMouse::VK_F12        => String::from("f12"),
        KeyboardAndMouse::VK_F13        => String::from("f13"),
        KeyboardAndMouse::VK_F14        => String::from("f14"),
        KeyboardAndMouse::VK_F15        => String::from("f15"),
        KeyboardAndMouse::VK_F16        => String::from("f16"),
        KeyboardAndMouse::VK_F17        => String::from("f17"),
        KeyboardAndMouse::VK_F18        => String::from("f18"),
        KeyboardAndMouse::VK_F19        => String::from("f19"),
        KeyboardAndMouse::VK_F20        => String::from("f20"),
        KeyboardAndMouse::VK_F21        => String::from("f21"),
        KeyboardAndMouse::VK_F22        => String::from("f22"),
        KeyboardAndMouse::VK_F23        => String::from("f23"),
        KeyboardAndMouse::VK_F24        => String::from("f24"),
        KeyboardAndMouse::VK_NUMLOCK    => String::from("numlock"),
        KeyboardAndMouse::VK_SCROLL     => String::from("scrolllock"),
        KeyboardAndMouse::VK_LSHIFT     => String::from("lshift"),
        KeyboardAndMouse::VK_RSHIFT     => String::from("rshift"),
        KeyboardAndMouse::VK_LCONTROL   => String::from("lctrl"),
        KeyboardAndMouse::VK_RCONTROL   => String::from("rctrl"),
        KeyboardAndMouse::VK_LMENU      => String::from("lalt"),
        KeyboardAndMouse::VK_RMENU      => String::from("ralt"),
        KeyboardAndMouse::VK_OEM_1      => String::from("semicolon"),
        KeyboardAndMouse::VK_OEM_PLUS   => String::from("equals"),
        KeyboardAndMouse::VK_OEM_COMMA  => String::from("comma"),
        KeyboardAndMouse::VK_OEM_MINUS  => String::from("minus"),
        KeyboardAndMouse::VK_OEM_PERIOD => String::from("period"),
        KeyboardAndMouse::VK_OEM_2      => String::from("forwardslash"),
        KeyboardAndMouse::VK_OEM_3      => String::from("backtick"),
        KeyboardAndMouse::VK_OEM_4      => String::from("leftbracket"),
        KeyboardAndMouse::VK_OEM_5      => String::from("backslash"),
        KeyboardAndMouse::VK_OEM_6      => String::from("rightbracket"),
        KeyboardAndMouse::VK_OEM_7      => String::from("apostrophe"),
        KeyboardAndMouse::VK_0          => String::from("0"),
        KeyboardAndMouse::VK_1          => String::from("1"),
        KeyboardAndMouse::VK_2          => String::from("2"),
        KeyboardAndMouse::VK_3          => String::from("3"),
        KeyboardAndMouse::VK_4          => String::from("4"),
        KeyboardAndMouse::VK_5          => String::from("5"),
        KeyboardAndMouse::VK_6          => String::from("6"),
        KeyboardAndMouse::VK_7          => String::from("7"),
        KeyboardAndMouse::VK_8          => String::from("8"),
        KeyboardAndMouse::VK_9          => String::from("9"),
        KeyboardAndMouse::VK_A          => String::from("a"),
        KeyboardAndMouse::VK_B          => String::from("b"),
        KeyboardAndMouse::VK_C          => String::from("c"),
        KeyboardAndMouse::VK_D          => String::from("d"),
        KeyboardAndMouse::VK_E          => String::from("e"),
        KeyboardAndMouse::VK_F          => String::from("f"),
        KeyboardAndMouse::VK_G          => String::from("g"),
        KeyboardAndMouse::VK_H          => String::from("h"),
        KeyboardAndMouse::VK_I          => String::from("i"),
        KeyboardAndMouse::VK_J          => String::from("j"),
        KeyboardAndMouse::VK_K          => String::from("k"),
        KeyboardAndMouse::VK_L          => String::from("l"),
        KeyboardAndMouse::VK_M          => String::from("m"),
        KeyboardAndMouse::VK_N          => String::from("n"),
        KeyboardAndMouse::VK_O          => String::from("o"),
        KeyboardAndMouse::VK_P          => String::from("p"),
        KeyboardAndMouse::VK_Q          => String::from("q"),
        KeyboardAndMouse::VK_R          => String::from("r"),
        KeyboardAndMouse::VK_S          => String::from("s"),
        KeyboardAndMouse::VK_T          => String::from("t"),
        KeyboardAndMouse::VK_U          => String::from("u"),
        KeyboardAndMouse::VK_V          => String::from("v"),
        KeyboardAndMouse::VK_W          => String::from("w"),
        KeyboardAndMouse::VK_X          => String::from("x"),
        KeyboardAndMouse::VK_Y          => String::from("y"),
        KeyboardAndMouse::VK_Z          => String::from("z"),
        _                               => format!("0x{:X}", vkey.0),
    }
}

// currently US layout only (409)
fn vkey_to_string(
    vk: KeyboardAndMouse::VIRTUAL_KEY,
    down: bool,
    shift: bool,
    alt: bool,
    ctrl: bool,
    caps: bool
) -> Option<String> {
    if alt || ctrl { return None; }

    if !down { return None; }

    match vk {
        // numpad
        KeyboardAndMouse::VK_ADD        => Some(String::from("+")),
        KeyboardAndMouse::VK_NUMPAD0    => if shift { None } else { Some(String::from("0")) },
        KeyboardAndMouse::VK_NUMPAD1    => if shift { None } else { Some(String::from("1")) },
        KeyboardAndMouse::VK_NUMPAD2    => if shift { None } else { Some(String::from("2")) },
        KeyboardAndMouse::VK_NUMPAD3    => if shift { None } else { Some(String::from("3")) },
        KeyboardAndMouse::VK_NUMPAD4    => if shift { None } else { Some(String::from("4")) },
        KeyboardAndMouse::VK_NUMPAD5    => if shift { None } else { Some(String::from("5")) },
        KeyboardAndMouse::VK_NUMPAD6    => if shift { None } else { Some(String::from("6")) },
        KeyboardAndMouse::VK_NUMPAD7    => if shift { None } else { Some(String::from("7")) },
        KeyboardAndMouse::VK_NUMPAD8    => if shift { None } else { Some(String::from("8")) },
        KeyboardAndMouse::VK_NUMPAD9    => if shift { None } else { Some(String::from("9")) },
        KeyboardAndMouse::VK_MULTIPLY   => Some(String::from("*")),
        KeyboardAndMouse::VK_SUBTRACT   => Some(String::from("-")),
        KeyboardAndMouse::VK_DECIMAL    => if shift { None } else { Some(String::from(".")) },
        KeyboardAndMouse::VK_DIVIDE     => Some(String::from("/")),

        // main keyboard
        KeyboardAndMouse::VK_SPACE      => Some(String::from(" ")),
        KeyboardAndMouse::VK_OEM_1      => Some(String::from(if shift { ":" } else { ";" })),
        KeyboardAndMouse::VK_OEM_PLUS   => Some(String::from(if shift { "+" } else { "=" })),
        KeyboardAndMouse::VK_OEM_COMMA  => Some(String::from(if shift { "<" } else { "," })),
        KeyboardAndMouse::VK_OEM_MINUS  => Some(String::from(if shift { "_" } else { "-" })),
        KeyboardAndMouse::VK_OEM_PERIOD => Some(String::from(if shift { ">" } else { "." })),
        KeyboardAndMouse::VK_OEM_2      => Some(String::from(if shift { "?" } else { "/" })),
        KeyboardAndMouse::VK_OEM_3      => Some(String::from(if shift { "~" } else { "`" })),
        KeyboardAndMouse::VK_OEM_4      => Some(String::from(if shift { "{" } else { "[" })),
        KeyboardAndMouse::VK_OEM_5      => Some(String::from(if shift { "|" } else { "\\" })),
        KeyboardAndMouse::VK_OEM_6      => Some(String::from(if shift { "}" } else { "]" })),
        KeyboardAndMouse::VK_OEM_7      => Some(String::from(if shift { "\"" } else { "'" })),
        KeyboardAndMouse::VK_0          => Some(String::from(if shift { ")" } else { "0" })),
        KeyboardAndMouse::VK_1          => Some(String::from(if shift { "!" } else { "1" })),
        KeyboardAndMouse::VK_2          => Some(String::from(if shift { "@" } else { "2" })),
        KeyboardAndMouse::VK_3          => Some(String::from(if shift { "#" } else { "3" })),
        KeyboardAndMouse::VK_4          => Some(String::from(if shift { "$" } else { "4" })),
        KeyboardAndMouse::VK_5          => Some(String::from(if shift { "%" } else { "5" })),
        KeyboardAndMouse::VK_6          => Some(String::from(if shift { "^" } else { "6" })),
        KeyboardAndMouse::VK_7          => Some(String::from(if shift { "&" } else { "7" })),
        KeyboardAndMouse::VK_8          => Some(String::from(if shift { "*" } else { "8" })),
        KeyboardAndMouse::VK_9          => Some(String::from(if shift { "(" } else { "9" })),
        KeyboardAndMouse::VK_A          => Some(String::from(if shift || caps { "A" } else { "a" })),
        KeyboardAndMouse::VK_B          => Some(String::from(if shift || caps { "B" } else { "b" })),
        KeyboardAndMouse::VK_C          => Some(String::from(if shift || caps { "C" } else { "c" })),
        KeyboardAndMouse::VK_D          => Some(String::from(if shift || caps { "D" } else { "d" })),
        KeyboardAndMouse::VK_E          => Some(String::from(if shift || caps { "E" } else { "e" })),
        KeyboardAndMouse::VK_F          => Some(String::from(if shift || caps { "F" } else { "f" })),
        KeyboardAndMouse::VK_G          => Some(String::from(if shift || caps { "G" } else { "g" })),
        KeyboardAndMouse::VK_H          => Some(String::from(if shift || caps { "H" } else { "h" })),
        KeyboardAndMouse::VK_I          => Some(String::from(if shift || caps { "I" } else { "i" })),
        KeyboardAndMouse::VK_J          => Some(String::from(if shift || caps { "J" } else { "j" })),
        KeyboardAndMouse::VK_K          => Some(String::from(if shift || caps { "K" } else { "k" })),
        KeyboardAndMouse::VK_L          => Some(String::from(if shift || caps { "L" } else { "l" })),
        KeyboardAndMouse::VK_M          => Some(String::from(if shift || caps { "M" } else { "m" })),
        KeyboardAndMouse::VK_N          => Some(String::from(if shift || caps { "N" } else { "n" })),
        KeyboardAndMouse::VK_O          => Some(String::from(if shift || caps { "O" } else { "o" })),
        KeyboardAndMouse::VK_P          => Some(String::from(if shift || caps { "P" } else { "p" })),
        KeyboardAndMouse::VK_Q          => Some(String::from(if shift || caps { "Q" } else { "q" })),
        KeyboardAndMouse::VK_R          => Some(String::from(if shift || caps { "R" } else { "r" })),
        KeyboardAndMouse::VK_S          => Some(String::from(if shift || caps { "S" } else { "s" })),
        KeyboardAndMouse::VK_T          => Some(String::from(if shift || caps { "T" } else { "t" })),
        KeyboardAndMouse::VK_U          => Some(String::from(if shift || caps { "U" } else { "u" })),
        KeyboardAndMouse::VK_V          => Some(String::from(if shift || caps { "V" } else { "v" })),
        KeyboardAndMouse::VK_W          => Some(String::from(if shift || caps { "W" } else { "w" })),
        KeyboardAndMouse::VK_X          => Some(String::from(if shift || caps { "X" } else { "x" })),
        KeyboardAndMouse::VK_Y          => Some(String::from(if shift || caps { "Y" } else { "y" })),
        KeyboardAndMouse::VK_Z          => Some(String::from(if shift || caps { "Z" } else { "z" })),
        _                               => None,
    }

}

