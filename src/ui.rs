// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT

//! UI state and internal API

pub mod font;
pub mod rect;
pub mod text;
pub mod window;
pub mod uibox;
pub mod grid;
pub mod separator;
pub mod button;
pub mod scrollview;
pub mod entry;
pub mod menu;

pub mod lua;

#[allow(unused_imports)]
use crate::logging::{debug, info, warn, error};
use crate::overlay;
use crate::input;
use crate::dx;

use std::sync::Arc;
use std::sync::Mutex;
use std::sync::atomic;

use std::collections::VecDeque;
use std::collections::HashMap;

use std::fs::File;
use std::io::BufRead;
use std::io::BufReader;

pub enum Element {
    Text(text::Text),
    Window(window::Window),
    Box(uibox::Box),
    Grid(grid::Grid),
    Separator(separator::Separator),
    Button(button::Button),
    ScrollView(scrollview::ScrollView),
    Entry(entry::Entry),
    Menu(menu::Menu),
    MenuItem(menu::MenuItem),
}

macro_rules! element_dispatch {
    ($self:expr, $fn_name:ident$(, $($args:tt)+)?) => {
        match $self {
            Element::Text(txt)      => txt.$fn_name($($($args)*)*),
            Element::Window(win)    => win.$fn_name($($($args)*)*),
            Element::Box(bx_)       => bx_.$fn_name($($($args)*)*),
            Element::Grid(grd)      => grd.$fn_name($($($args)*)*),
            Element::Separator(sep) => sep.$fn_name($($($args)*)*),
            Element::Button(btn)    => btn.$fn_name($($($args)*)*),
            Element::ScrollView(sv) =>  sv.$fn_name($($($args)*)*),
            Element::Entry(ent)     => ent.$fn_name($($($args)*)*),
            Element::Menu(men)      => men.$fn_name($($($args)*)*),
            Element::MenuItem(mi)   =>  mi.$fn_name($($($args)*)*),
        }
    }
}

impl Element {
    pub fn draw(self: &Arc<Self>, offset_x: i64, offset_y: i64, frame: &mut dx::SwapChainLock) {
        element_dispatch!(&**self, draw, offset_x, offset_y, frame, self);
    }

    pub fn process_mouse_event(self: &Arc<Self>, offset_x: i64, offset_y: i64, event: &input::MouseEvent) -> bool {
        element_dispatch!(&**self, process_mouse_event, offset_x, offset_y, &event, self)
    }

    pub fn process_keyboard_event(&self, event: &input::KeyboardEvent) -> bool {
        element_dispatch!(self, process_keyboard_event, event)
    }

    pub fn get_x(&self) -> i64 {
        element_dispatch!(self, get_x)
    }

    pub fn set_x(&self, value: i64) {
        element_dispatch!(self, set_x, value)
    }

    pub fn get_y(&self) -> i64 {
        element_dispatch!(self, get_y)
    }

    pub fn set_y(&self, value: i64) {
        element_dispatch!(self, set_y, value);
    }

    pub fn get_preferred_width(&self) -> i64 {
        element_dispatch!(self, get_preferred_width)
    }

    pub fn get_preferred_height(&self) -> i64 {
        element_dispatch!(self, get_preferred_height)
    }

    pub fn get_width(&self) -> i64 {
        element_dispatch!(self, get_width)
    }

    pub fn set_width(&self, value: i64) {
        element_dispatch!(self, set_width, value);
    }

    pub fn get_height(&self) -> i64 {
        element_dispatch!(self, get_height)
    }

    pub fn set_height(&self, value: i64) {
        element_dispatch!(self, set_height, value);
    }

    pub fn get_bg_color(&self) -> Color {
        element_dispatch!(self, get_bg_color)
    }

    pub fn set_bg_color(&self, value: Color) {
        element_dispatch!(self, set_bg_color, value);
    }

    pub fn as_text(&self) -> Option<&text::Text> {
        match &self {
            Element::Text(t) => Some(t),
            _                => None,
        }
    }

    pub fn as_window(&self) -> Option<&window::Window> {
        match &self {
            Element::Window(w) => Some(w),
            _                  => None,
        }
    }

    pub fn as_box(&self) -> Option<&uibox::Box> {
        match &self {
            Element::Box(b) => Some(b),
            _               => None,
        }
    }

    pub fn as_grid(&self) -> Option<&grid::Grid> {
        match &self {
            Element::Grid(g) => Some(g),
            _                => None,
        }
    }

    pub fn as_separator(&self) -> Option<&separator::Separator> {
        match &self {
            Element::Separator(s) => Some(s),
            _                     => None,
        }
    }

    pub fn as_button(&self) -> Option<&button::Button> {
        match &self {
            Element::Button(b) => Some(b),
            _                  => None,
        }
    }
    
    pub fn as_scrollview(&self) -> Option<&scrollview::ScrollView> {
        match &self {
            Element::ScrollView(s) => Some(s),
            _                      => None,
        }
    }

    pub fn as_entry(&self) -> Option<&entry::Entry> {
        match &self {
            Element::Entry(e) => Some(e),
            _                 => None,
        }
    }

    pub fn as_menu(&self) -> Option<&menu::Menu> {
        match &self {
            Element::Menu(m) => Some(m),
            _                => None,
        }
    }

    pub fn as_menuitem(&self) -> Option<&menu::MenuItem> {
        match &self {
            Element::MenuItem(m) => Some(m),
            _                    => None,
        }
    }
}

/// The global state for the UI
pub struct Ui {
    top_level_elements: Mutex<VecDeque<Arc<Element>>>,

    // events are run against the elements that registered for them in the prior
    // frame. this is done because an element may have input_elements locked
    // while drawing and a mouse event is being processed at the same time,
    // which would cause a mutex deadlock
    input_elements: Mutex<VecDeque<InputElement>>,
    input_elements_last_frame: Mutex<VecDeque<InputElement>>,
    mouse_over_element: Mutex<Option<InputElement>>,
    mouse_capture_element: Mutex<Option<InputElement>>,

    focus_element: Mutex<Option<Arc<Element>>>,

    last_mouse_x: atomic::AtomicI64,
    last_mouse_y: atomic::AtomicI64,

    last_ui_size: Mutex<(u32, u32)>,

    // fonts must be declared before the font manager so that they are dropped
    // first.
    pub regular_font: Arc<font::Font>,
    pub mono_font: Arc<font::Font>,
    pub icon_font: Arc<font::Font>,

    icon_codepoint_cache: Mutex<HashMap<String, String>>,

    pub font_manager: font::FontManager,
    pub rect: rect::Rect,   
}

fn get_default_font(font_manager: &font::FontManager, key: &str) -> Arc<font::Font> {
    let o_settings = overlay::settings();
    let path = o_settings.get_string(format!("overlay.ui.font.{}.path", key).as_str()).unwrap();
    let size = o_settings.get_u64(format!("overlay.ui.font.{}.size", key).as_str()).unwrap() as u32;
    let vars_obj = o_settings.get_object(format!("overlay.ui.font.{}.vars", key).as_str()).unwrap();
    let mut vars: Vec<(String, i32)> = Vec::new();

    debug!("  {}: {}", key, path);
    debug!("    size: {}", size);
    for (axis, val) in vars_obj.iter() {
        debug!("    {}: {}", axis, val);
        let ival = val.as_i64().unwrap() as i32;
        vars.push((axis.clone(), ival));
    }

    font_manager.get_font(path.as_str(), size, &vars)
}

impl Ui {
    /// Initializes the UI.
    pub fn new() -> Arc<Ui> {
        debug!("init");

        let o_settings = overlay::settings();

        // default style
        o_settings.set_default_value("overlay.ui.colors.windowBG"             , 0x000000bbu32);
        o_settings.set_default_value("overlay.ui.colors.windowBorder"         , 0x3D4478FFu32);
        o_settings.set_default_value("overlay.ui.colors.windowBorderHighlight", 0x3d5a78ffu32);

        o_settings.set_default_value("overlay.ui.colors.text",                  0xFFFFFFFFu32);
        o_settings.set_default_value("overlay.ui.colors.accentText",            0xFCBA03FFu32);

        o_settings.set_default_value("overlay.ui.colors.entryBG",               0x262626FFu32);
        o_settings.set_default_value("overlay.ui.colors.entryHint",             0x707070FFu32);
        o_settings.set_default_value("overlay.ui.entryFocusCaret",              "\u{2588}");
        o_settings.set_default_value("overlay.ui.entryInactiveCaret",           "\u{2591}");

        o_settings.set_default_value("overlay.ui.colors.buttonBG",              0x1F253BDDu32);
        o_settings.set_default_value("overlay.ui.colors.buttonBGHover",         0x2E3859FFu32);
        o_settings.set_default_value("overlay.ui.colors.buttonBGHighlight",     0x3a4670FFu32);
        o_settings.set_default_value("overlay.ui.colors.buttonBorder",          0x3D4478FFu32);

        o_settings.set_default_value("overlay.ui.colors.scrollThumb",           0x3D4478FFu32);
        o_settings.set_default_value("overlay.ui.colors.scrollThumbHighlight",  0x3d5a78ffu32);
        o_settings.set_default_value("overlay.ui.colors.scrollBG",              0x1E202ECCu32);

        o_settings.set_default_value("overlay.ui.colors.menuBG",                0x161a26DDu32);
        o_settings.set_default_value("overlay.ui.colors.menuBorder",            0x3D4478FFu32);
        o_settings.set_default_value("overlay.ui.colors.menuItemHover",         0x2E3859FFu32);
        o_settings.set_default_value("overlay.ui.colors.menuItemHighlight",     0x3a4670FFu32);

        o_settings.set_default_value("overlay.ui.font.gammaCorrection", 1.4);
        o_settings.set_default_value("overlay.ui.font.regular.path"     , "fonts/Inter.ttf");
        o_settings.set_default_value("overlay.ui.font.regular.size"     ,  12);
        o_settings.set_default_value("overlay.ui.font.regular.vars", serde_json::json!({
            "wght": 400,
            "slnt": 0
        }));

        o_settings.set_default_value("overlay.ui.font.mono.path", "fonts/CascadiaCode.ttf");
        o_settings.set_default_value("overlay.ui.font.mono.size",  12);
        o_settings.set_default_value("overlay.ui.font.mono.vars", serde_json::json!({
            "wght": 350,
        }));

        o_settings.set_default_value("overlay.ui.font.icon.path"  , "fonts/MaterialSymbolsOutlined.ttf");
        o_settings.set_default_value("overlay.ui.font.icon.size"  , 12);
        o_settings.set_default_value("overlay.ui.font.icon.vars", serde_json::json!({
            "wght": 500,
        }));
        o_settings.set_default_value("overlay.ui.font.icon.codepoints", "fonts/MaterialSymbolsOutlined.codepoints");
        
        // load default fonts
        let font_man = font::FontManager::new();

        debug!("Loading default fonts:");
        let reg_font = get_default_font(&font_man, "regular");
        let mono_font = get_default_font(&font_man, "mono");
        let icon_font = get_default_font(&font_man, "icon");

        let ui = Ui {
            top_level_elements: Mutex::new(VecDeque::new()),
            input_elements: Mutex::new(VecDeque::new()),
            input_elements_last_frame: Mutex::new(VecDeque::new()),
            mouse_over_element: Mutex::new(None),
            mouse_capture_element: Mutex::new(None),

            focus_element: Mutex::new(None),

            last_mouse_x: atomic::AtomicI64::new(0),
            last_mouse_y: atomic::AtomicI64::new(0),

            last_ui_size: Mutex::new((0, 0)),

            font_manager: font_man,
            rect: rect::Rect::new(),

            regular_font: reg_font,
            mono_font: mono_font,
            icon_font: icon_font,

            icon_codepoint_cache: Mutex::new(HashMap::new())
        };

        
        crate::lua_manager::add_module_opener("eg-overlay-ui", Some(crate::ui::lua::open_module));

        return Arc::new(ui);
    }

    pub fn add_top_level_element(&self, element: &Arc<Element>) {
        let mut top_level = self.top_level_elements.lock().unwrap();

        for tle in &*top_level {
            if Arc::ptr_eq(tle, element) {
                // element is already top level
                warn!("Element is already top level.");
                return;
            }
        }

        top_level.push_back(element.clone());
    }

    pub fn remove_top_level_element(&self, element: &Arc<Element>) {
        let mut top_level = self.top_level_elements.lock().unwrap();

        let mut ind: Option<usize> = None;

        let mut i: usize = 0;
        for tle in &*top_level {
            if Arc::ptr_eq(tle, element) {
                ind = Some(i);
                break;
            }
            i += 1;
        }

        if let Some(i) = ind {
            let _ = top_level.remove(i);
        }/* else {
            warn!("Element was not top level.");
        }*/
    }

    pub fn move_element_to_top(&self, element: &Arc<Element>) {
        // same as remove + add but with a single mutex lock
        let mut top_level = self.top_level_elements.lock().unwrap();

        let mut ind: Option<usize> = None;

        let mut i: usize = 0;
        for tle in &*top_level {
            if Arc::ptr_eq(tle, element) {
                ind = Some(i);
                break;
            }
            i += 1;
        }

        if let Some(i) = ind {
            let _ = top_level.remove(i);
        }

        top_level.push_back(element.clone());
    }

    pub fn add_input_element(&self, element: &Arc<Element>, offset_x: i64, offset_y: i64) {
        let e = element.clone();

        let ie = InputElement {
            element: e,
            offset_x: offset_x,
            offset_y: offset_y,
        };

        self.input_elements.lock().unwrap().push_front(ie);
    }

    pub fn set_mouse_capture(&self, element: &Arc<Element>, offset_x: i64, offset_y: i64) {
        if self.mouse_capture_element.lock().unwrap().is_some() { return; }

        *self.mouse_capture_element.lock().unwrap() = Some(InputElement {
            element: element.clone(),
            offset_x: offset_x,
            offset_y: offset_y,
        });
    }

    pub fn clear_mouse_capture(&self) {
        *self.mouse_capture_element.lock().unwrap() = None;
    }

    pub fn draw(&self, frame: &mut crate::dx::SwapChainLock) {
        let mut ui_size = self.last_ui_size.lock().unwrap();
        ui_size.0 = frame.render_target_width();
        ui_size.1 = frame.render_target_height();
        drop(ui_size);

        //self.input_elements.lock().unwrap().clear();
        let mut ielf = self.input_elements_last_frame.lock().unwrap();

        ielf.clear();

        for ie in self.input_elements.lock().unwrap().drain(..) {
            ielf.push_back(ie);
        }

        let top_level = self.top_level_elements.lock().unwrap();

        for e in &*top_level {
            e.draw(0, 0, frame);
        }
    }

    pub fn process_mouse_event(&self, event: &input::MouseEvent) -> bool {
        // capture mouse location
        self.last_mouse_x.store(event.x(), atomic::Ordering::Relaxed);
        self.last_mouse_y.store(event.y(), atomic::Ordering::Relaxed);

        if let input::MouseEvent::Button(b) = event {
            if b.down {
                self.set_focus_element(None);
            }
        }

        let mut e_under_mouse: Option<&InputElement> = None;

        let input_elements = self.input_elements_last_frame.lock().unwrap();

        for ie in &*input_elements {
            if ie.pos_within(event.x() as i64, event.y() as i64) {
                e_under_mouse = Some(ie);
                break;
            }
        }

        if let Some(e) = e_under_mouse { // there is an element currently under the mouse
            if let Some(moe) = self.mouse_over_element.lock().unwrap().as_ref() {
                // there was something under the mouse during the last event
                if !Arc::ptr_eq(&moe.element, &e.element) {
                    // the current element isn't what was under the mouse before
                    //debug!("Sending mouse event to element (leave): {}", event);
                    moe.element.process_mouse_event(moe.offset_x, moe.offset_y, &event.as_leave());
                    e.element.process_mouse_event(e.offset_x, e.offset_y, &event.as_enter());
                }
            } else {
                //debug!("Sending mouse event to element (enter): {}", event);
                e.element.process_mouse_event(e.offset_x, e.offset_y, &event.as_enter());
            }
            *self.mouse_over_element.lock().unwrap() = Some(e.clone());

        } else if self.mouse_over_element.lock().unwrap().is_some() {

            if let Some(moe) = self.mouse_over_element.lock().unwrap().as_ref() {
                // there isn't an element under the mouse, but there was during the last event
                //debug!("Sending mouse event to element (leave2): {}", event);
                moe.element.process_mouse_event(moe.offset_x, moe.offset_y, &event.as_leave());
            }
            // can't do this within the if let because the mutex is locked within that scope
            *self.mouse_over_element.lock().unwrap() = None;
        }

        if self.mouse_capture_element.lock().unwrap().is_some() {
            // give the mouse capture element a chance to process the event first
            // mouse_capture_element cant be locked during this call because it
            // may remove itself from capture
            let lock = self.mouse_capture_element.lock().unwrap();
            let mce = lock.as_ref().unwrap();

            let e = mce.element.clone();
            let offx = mce.offset_x;
            let offy = mce.offset_y;

            drop(lock);
            //debug!("Sending mouse event to element (capture): {}", event);
            if e.process_mouse_event(offx, offy, &event) {
                return true;
            }
        }

        for ie in &*input_elements {
            if ie.pos_within(event.x() as i64, event.y() as i64) {    
                //debug!("Sending mouse event to element: {}", event);
                if ie.element.process_mouse_event(ie.offset_x, ie.offset_y, &event) {
                    return true;
                }
            }
        }
        
        false
    }

    pub fn process_keyboard_event(&self, event: &input::KeyboardEvent) -> bool {
        if let Some(e) = self.focus_element.lock().unwrap().as_ref() {
            if e.process_keyboard_event(event) {
                return true
            } else {
                return crate::lua_manager::process_keybinds(event);
            }
        }

        return crate::lua_manager::process_keybinds(event);
    }

    pub fn set_focus_element(&self, element: Option<Arc<Element>>) {
        *self.focus_element.lock().unwrap() = element;
    }

    pub fn element_is_focus(&self, element: &Arc<Element>) -> bool {
        if let Some(fe) = self.focus_element.lock().unwrap().as_ref() {
            Arc::ptr_eq(element, fe)
        } else {
            false
        }
    }

    pub fn icon_codepoint(&self, name: &str) -> Result<String, &'static str> {
        let mut cache = self.icon_codepoint_cache.lock().unwrap();

        if let Some(cp) = cache.get(name) {
            return Ok(cp.clone());
        }

        let settings = overlay::settings();

        let codepointpath = settings.get_string("overlay.ui.font.icon.codepoints").unwrap();

        let file: File;
        if let Ok(f) = File::open(codepointpath){
            file = f;
        } else {
            return Err("Couldn't open icon codepoints.");
        }

        let buf = BufReader::new(file);

        for line_r in buf.lines() {
            match line_r {
                Ok(line) => {
                    let parts: Vec<_> = line.split(" ").collect();
                    if parts[0]==name {
                        if let Ok(cp) = u32::from_str_radix(parts[1], 16) {
                            let cpstr = unsafe { char::from_u32_unchecked(cp) }.to_string();
                            cache.insert(String::from(name), cpstr.clone());
                            return Ok(cpstr);
                        } else {
                            return Err("couldn't parse codepoint value.");
                        }
                    }
                },
                Err(_) => return Err("error reading codepoints."),
            }
        }

        Err("Couldn't find codepoint.")
    }

    pub fn get_last_mouse_x(&self) -> i64 {
        self.last_mouse_x.load(atomic::Ordering::Relaxed)
    }

    pub fn get_last_mouse_y(&self) -> i64 {
        self.last_mouse_y.load(atomic::Ordering::Relaxed)
    }
}

impl Drop for Ui {
    fn drop(&mut self) {
        debug!("cleanup");
    }
}

/// A 32-bit color.
///
/// Colors are 32bit integers, stored in RGBA format.  
/// This means that they can be conveniently conveyed in hex format,
/// a.k.a. HTML colors. ie. red = 0xFF0000FF
#[derive(Copy,Clone)]
pub struct Color(u32);

impl From<u32> for Color {
    fn from(val: u32) -> Self {
        Self(val)
    }
}

impl Into<u32> for Color {
    fn into(self) -> u32 {
        self.0
    }
}

impl From<i64> for Color {
    fn from(val: i64) -> Self {
        Self(val as u32)
    }
}

impl Into<i64> for Color {
    fn into(self) -> i64 {
        self.0 as i64
    }
}

impl Color {
    /// Returns the red component of the color as a value between 0 and 255.
    pub fn r_u8(&self) -> u8 { ((self.0 >> 24) & 0xFF) as u8 }

    /// Returns the green component of the color as a value between 0 and 255.
    pub fn g_u8(&self) -> u8 { ((self.0 >> 16) & 0xFF) as u8 }

    /// Returns the blue component of the color as a value between 0 and 255.
    pub fn b_u8(&self) -> u8 { ((self.0 >>  8) & 0xFF) as u8 }

    /// Returns the alpha component of the color as a value between 0 and 255.
    pub fn a_u8(&self) -> u8 { ( self.0        & 0xFF) as u8 }

    /// Returns the red component of the color as a value between 0.0 and 1.0.
    pub fn r_f32(&self) -> f32 { self.r_u8() as f32 / 255.0f32 }

    /// Returns the green component of the color as a value between 0.0 and 1.0.
    pub fn g_f32(&self) -> f32 { self.g_u8() as f32 / 255.0f32 }

    /// Returns the blue component of the color as a value between 0.0 and 1.0.
    pub fn b_f32(&self) -> f32 { self.b_u8() as f32 / 255.0f32 }

    /// Returns the alpha component of the color as a value between 0.0 and 1.0.
    pub fn a_f32(&self) -> f32 { self.a_u8() as f32 / 255.0f32 }
}

#[derive(Clone)]
struct InputElement {
    offset_x: i64,
    offset_y: i64,

    element: Arc<Element>,
}

impl InputElement {
    pub fn pos_within(&self, x: i64, y: i64) -> bool {
        let e = &self.element;
        let left = e.get_x() + self.offset_x;
        let top = e.get_y() + self.offset_y;
        let bottom = top + e.get_height();
        let right = left + e.get_width();

        x >= left  &&
        x <= right &&
        y >= top   &&
        y <= bottom
    }
}

#[derive(PartialEq)]
pub enum ElementAlignment {
    Start,
    Middle,
    End,
    Fill,
}

impl From<&str> for ElementAlignment {
    fn from(name: &str) -> Self {
        match name {
            "start"  => ElementAlignment::Start,
            "middle" => ElementAlignment::Middle,
            "end"    => ElementAlignment::End,
            "fill"   => ElementAlignment::Fill,
            _=> {
                warn!("Unknown ElementAlignment: {}", name);
                ElementAlignment::Start
            },
        }
    }
}

#[derive(PartialEq)]
pub enum ElementOrientation {
    Horizontal,
    Vertical,
}
