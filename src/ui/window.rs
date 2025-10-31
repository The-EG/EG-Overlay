// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT
use std::sync::Arc;
use std::sync::Mutex;
use std::sync::Weak;

#[allow(unused_imports)]
use crate::logging::{debug, info, warn, error};

use crate::ui;
use crate::overlay;
use crate::input;
use crate::settings;

use windows::Win32::Foundation;

pub mod lua;

pub struct Window {
    win: Mutex<WindowInner>,
}

struct WindowInner {
    caption: String,

    pref_width: i64,
    pref_height: i64,

    x: i64,
    y: i64,
    min_width: Option<i64>,
    min_height: Option<i64>,
    width: i64,
    height:i64,

    titlebar_height: i64,

    resizable: bool,
    show_titlebar: bool,

    titlebar_box: Arc<ui::Element>,

    // highlight the toolbar when the mouse hovers over it
    hover_titlebar: bool,

    // highlight the border segments when moves hovers and during resize
    hover_right: bool,
    hover_left: bool,
    hover_bottom: bool,

    moving: bool,
    resizing: bool,

    // these are also used for resizing
    move_last_x: i64,
    move_last_y: i64,

    bg_color: ui::Color,
    border_color: ui::Color,
    border_highlight_color: ui::Color,

    child: Option<Arc<ui::Element>>,

    settings: Option<Arc<settings::SettingsStore>>,
    settings_path: Option<String>,

    // needed for adding this window as a mouse capture element
    last_scissor: Foundation::RECT,

    events: bool,

    ui: Weak<ui::Ui>,
}

impl Window {
    pub fn new(caption: &str) -> Arc<ui::Element> {
        let o_settings = crate::overlay::settings();

        let bg_color = ui::Color::from(o_settings.get_u64("overlay.ui.colors.windowBG").unwrap() as u32);
        let border_color = ui::Color::from(o_settings.get_u64("overlay.ui.colors.windowBorder").unwrap() as u32);
        let border_highlight_color = ui::Color::from(o_settings.get_u64("overlay.ui.colors.windowBorderHighlight").unwrap() as u32);

        let win = Mutex::new(WindowInner {
            caption: String::from(caption),

            pref_width: 0,
            pref_height: 0,

            x: 0,
            y: 0,
            min_width: None,
            min_height: None,
            width: 10,
            height: 10,

            titlebar_height: overlay::ui().regular_font.get_line_spacing() as i64 + 2,

            resizable: false,
            show_titlebar: true,

            titlebar_box: ui::uibox::Box::new(ui::ElementOrientation::Horizontal),

            hover_titlebar: false,
            hover_right: false,
            hover_left: false,
            hover_bottom: false,

            moving: false,
            resizing: false,

            move_last_x: 0,
            move_last_y: 0,

            bg_color: bg_color,
            border_color: border_color,
            border_highlight_color: border_highlight_color,

            child: None,

            settings: None,
            settings_path: None,

            last_scissor: Foundation::RECT::default(),

            events: true,

            ui: Arc::downgrade(&overlay::ui()),
        });

        Arc::new(ui::Element::Window(Window { win: win }))
    }

    pub fn draw(
        &self,
        offset_x: i64,
        offset_y: i64,
        frame: &mut crate::dx::SwapChainLock,
        element: &Arc<ui::Element>
    ) {
        self.win.lock().unwrap().draw(offset_x, offset_y, frame, element);
    }

    pub fn process_mouse_event(
        &self,
        offset_x: i64,
        offset_y: i64,
        event: &input::MouseEvent,
        element: &Arc<ui::Element>
    ) -> bool {
        self.win.lock().unwrap().process_mouse_event(offset_x, offset_y, event, element)
    }

    pub fn process_keyboard_event(&self, _event: &input::KeyboardEvent) -> bool {
        false
    }

    pub fn get_preferred_width(&self) -> i64 {
        self.win.lock().unwrap().pref_width
    }

    pub fn get_preferred_height(&self) -> i64 {
        self.win.lock().unwrap().pref_height
    }

    pub fn get_x(&self) -> i64 {
        self.win.lock().unwrap().x
    }

    pub fn set_x(&self, x: i64) {
        self.win.lock().unwrap().x = x;
    }

    pub fn get_y(&self) -> i64 {
        self.win.lock().unwrap().y
    }

    pub fn set_y(&self, y: i64) {
        self.win.lock().unwrap().y = y;
    }

    pub fn get_width(&self) -> i64 {
        self.win.lock().unwrap().width
    }

    pub fn set_width(&self, width: i64) {
        self.win.lock().unwrap().width = width;
    }

    pub fn get_height(&self) -> i64 {
        self.win.lock().unwrap().height
    }

    pub fn set_height(&self, height: i64) {
        self.win.lock().unwrap().height = height;
    }

    pub fn get_bg_color(&self) -> ui::Color {
        self.win.lock().unwrap().bg_color
    }

    pub fn set_bg_color(&self, color: ui::Color) {
        self.win.lock().unwrap().bg_color = color;
    }

    pub fn on_lost_focus(&self) { }
}

impl WindowInner {
    fn draw_decorations(&mut self, offset_x: i64, offset_y: i64, frame: &mut crate::dx::SwapChainLock) {
        let win_x = offset_x + self.x;
        let win_y = offset_y + self.y;

        let win_w = self.width;
        let win_h = self.height;

        let r = &self.ui.upgrade().unwrap().rect;

        // background
        r.draw(frame, win_x, win_y, win_w, win_h, self.bg_color);

        // borders
        let border_color = self.border_color;
        let border_highlight_color = self.border_highlight_color;

        let title_color  = if self.hover_titlebar { border_highlight_color } else { border_color };
        let left_color   = if self.hover_left     { border_highlight_color } else { border_color };
        let right_color  = if self.hover_right    { border_highlight_color } else { border_color };
        let bottom_color = if self.hover_bottom   { border_highlight_color } else { border_color };

        let left_width   = if self.hover_left   { 3 } else { 1 };
        let right_width  = if self.hover_right  { 3 } else { 1 };
        let bottom_width = if self.hover_bottom { 3 } else { 1 };

        r.draw(frame, win_x, win_y, left_width, win_h, left_color);               // left
        r.draw(frame, win_x, win_y + win_h - bottom_width, win_w, bottom_width, bottom_color); // bottom
        r.draw(frame, win_x + win_w - right_width, win_y, right_width, win_h, right_color);  // right

        if self.show_titlebar {
            let mut box_w = self.titlebar_box.get_preferred_width();
            let box_h = self.titlebar_box.get_preferred_height();

            self.titlebar_height = self.ui.upgrade().unwrap().regular_font.get_line_spacing() as i64 + 2;

            // adjust titlebar height if the box has items
            if box_w > 0 && box_h + 2 > self.titlebar_height {
                self.titlebar_height = box_h + 2;
            }

            // titlebar
            r.draw(frame, win_x, win_y, win_w, self.titlebar_height, title_color);

            // caption
            let cap_x = win_x + 3;
            let cap_y = win_y + 1;
            let cap_w = win_w - 6;
            let cap_h = self.titlebar_height;

            if frame.push_scissor(cap_x, cap_y, cap_x + cap_w, cap_y + cap_h) {
                let f = self.ui.upgrade().unwrap().regular_font.clone();

                let text_y = cap_y + (cap_h as f64 / 2.0) as i64 - (f.get_line_spacing() as f64 / 2.0) as i64;
                f.render_text(frame, cap_x, text_y, &self.caption, ui::Color::from(0xFFFFFFFFu32));

                if box_w > 0 {
                    if box_w > cap_w { box_w = cap_w; }

                    self.titlebar_box.set_width(box_w);
                    self.titlebar_box.set_height(box_h);

                    let box_x = cap_x + cap_w - box_w - 1;
                    let box_y = cap_y;

                    self.titlebar_box.draw(box_x, box_y, frame);
                }

                frame.pop_scissor();
            }
        } else {
            // top border
            r.draw(frame, win_x, win_y, win_w, 1, title_color);
        }
    }

    fn on_mouse_move(&mut self, offset_x: i64, offset_y: i64, event: &input::MouseGenericEvent) -> bool {
        let winx = self.x + offset_x;
        let winy = self.y + offset_y;

        let titlebar_bottom = winy + self.titlebar_height;

        if self.show_titlebar &&
           event.x >= winx && event.y >= winy &&
           event.x <= winx + self.width &&
           event.y <= titlebar_bottom
        {
            self.hover_titlebar = true;
        } else if !self.moving {
            self.hover_titlebar = false;
        }

        if !self.hover_titlebar && self.resizable &&
           event.x >= (winx + self.width - 4) && event.x <= (winx + self.width) &&
           event.y >= winy && event.y <= winy + self.height
        {
            self.hover_right = true;
        } else if !self.resizing {
            self.hover_right = false;
        }

        if !self.hover_titlebar && self.resizable &&
           event.x >= winx && event.x <= winx + 4 &&
           event.y >= winy && event.y <= winy + self.height
        {
            self.hover_left = true;
        } else if !self.resizing {
            self.hover_left = false;
        }

        if self.resizable && event.x >= winx && event.x < winx + self.width &&
           event.y >= winy + self.height - 4 && event.y <= winy + self.height
        {
            self.hover_bottom = true;
        } else if !self.resizing {
            self.hover_bottom = false;
        }

        if self.moving {
            self.x += event.x - self.move_last_x;
            self.y += event.y - self.move_last_y;
        } else if self.resizing {
            if self.hover_left {
                self.x     += event.x - self.move_last_x;
                self.width -= event.x - self.move_last_x;
            }
            if self.hover_right { self.width += event.x - self.move_last_x; }
            if self.hover_bottom { self.height += event.y - self.move_last_y; }

            if self.height < self.min_height.unwrap_or(10) {
                self.height = self.min_height.unwrap_or(10);
            }
            if self.width < self.min_width.unwrap_or(10) {
                self.width = self.min_width.unwrap_or(10);
            }
        }

        self.move_last_x = event.x;
        self.move_last_y = event.y;

        true
    }

    fn on_mouse_leave(&mut self) -> bool {
        if !self.moving { self.hover_titlebar = false; }

        if !self.resizing {
            self.hover_right    = false;
            self.hover_left     = false;
            self.hover_bottom   = false;
        }

        true
    }

    fn on_mouse_button(
        &mut self,
        offset_x: i64,
        offset_y: i64,
        event: &input::MouseButtonEvent,
        element: &Arc<ui::Element>
    ) -> bool {
        if event.down {
            overlay::ui().move_element_to_top(element);
        }

        if !self.moving && self.hover_titlebar && event.button == input::MouseButtonEventButton::Left && event.down {
            self.moving = true;
            self.move_last_x = event.x;
            self.move_last_y = event.y;
            overlay::ui().set_mouse_capture(element, offset_x, offset_y, self.last_scissor.clone());
        } else if self.moving && event.button == input::MouseButtonEventButton::Left && !event.down {
            self.moving = false;
            overlay::ui().clear_mouse_capture();
            self.save_to_settings();
        } else if !self.resizing &&
                  (self.hover_right || self.hover_bottom || self.hover_left) &&
                  event.button == input::MouseButtonEventButton::Left &&
                  event.down
        {
            self.resizing = true;
            self.move_last_x = event.x;
            self.move_last_y = event.y;
            overlay::ui().set_mouse_capture(element, offset_x, offset_y, self.last_scissor.clone());
        } else if self.resizing && event.button == input::MouseButtonEventButton::Left && !event.down {
            self.resizing = false;
            overlay::ui().clear_mouse_capture();
            self.save_to_settings();
        }

        true
    }

    fn update_size(&mut self) {
        if !self.resizable && self.child.is_some() {
            let child_width = self.child.as_ref().unwrap().get_preferred_width();
            let child_height = self.child.as_ref().unwrap().get_preferred_height();

            self.width = child_width + 4;
            if self.show_titlebar {
                self.height = child_height + self.titlebar_height + 3;
            } else {
                self.height = child_height + 4;
            }
        }
    }

    pub fn draw(
        &mut self,
        offset_x: i64,
        offset_y: i64,
        frame: &mut crate::dx::SwapChainLock,
        element: &Arc<ui::Element>
    ) {
        let child_width:i64;
        let child_height:i64;

        let orig_width = self.width;
        let orig_height = self.height;

        if !self.resizable && self.child.is_some() {
            child_width = self.child.as_ref().unwrap().get_preferred_width();
            child_height = self.child.as_ref().unwrap().get_preferred_height();

            self.width = child_width + 4;
            if self.show_titlebar {
                self.height = child_height + self.titlebar_height + 3;
            } else {
                self.height = child_height + 4;
            }
        } else {
            child_width = self.width;
            child_height = self.height;
        }

        if let Some(min_w) = self.min_width {
            if self.width < min_w {
                self.width = min_w;
            }
        }

        if let Some(min_h) = self.min_height {
            if self.height < min_h {
                self.height = min_h;
            }
        }

        if self.child.is_some() {
            let c = self.child.as_ref().unwrap();
            c.set_width(self.width - 4);
            if self.show_titlebar {
                c.set_height(self.height - self.titlebar_height - 3);
            } else {
                c.set_height(self.height - 4);
            }

        }


        self.last_scissor = frame.current_scissor();
        if self.events {
            self.ui.upgrade().unwrap().add_input_element(element, offset_x, offset_y, self.last_scissor.clone());
        }

        self.draw_decorations(offset_x, offset_y, frame);

        if let Some(child) = &self.child {
            let coffx = offset_x + self.x + 2;
            let coffy = offset_y + self.y + if self.show_titlebar { self.titlebar_height } else { 2 };

            if frame.push_scissor(coffx, coffy, coffx + child_width, coffy + child_height) {
                child.draw(coffx, coffy, frame);
                frame.pop_scissor();
            }
        }

        if orig_width != self.width || orig_height != self.height {
            self.save_to_settings();
        }
    }

    pub fn process_mouse_event(
        &mut self,
        offset_x: i64,
        offset_y: i64,
        event: &input::MouseEvent,
        element: &Arc<ui::Element>
    ) -> bool {
        match event {
            input::MouseEvent::Move(mv) => return self.on_mouse_move(offset_x, offset_y, mv),
            input::MouseEvent::Leave(_) => return self.on_mouse_leave(),
            input::MouseEvent::Button(btn) => return self.on_mouse_button(offset_x, offset_y, btn, element),
            _ => {},
        }

        true
    }

    pub fn update_from_settings(&mut self) {
        if self.settings.is_none() { return; }
        let s = self.settings.as_ref().unwrap();
        let path_prefix = self.settings_path.as_ref().unwrap();

        let x = s.get_i64(format!("{}.x"     , path_prefix).as_str()).unwrap_or(0);
        let y = s.get_i64(format!("{}.y"     , path_prefix).as_str()).unwrap_or(0);
        let w = s.get_i64(format!("{}.width" , path_prefix).as_str()).unwrap_or(0);
        let h = s.get_i64(format!("{}.height", path_prefix).as_str()).unwrap_or(0);

        self.x = x;
        self.y = y;
        self.width = w;
        self.height = h;
    }

    pub fn save_to_settings(&self) {
        if self.settings.is_none() { return; }

        let s = self.settings.as_ref().unwrap();
        let path_prefix = self.settings_path.as_ref().unwrap();

        s.set(format!("{}.x"     , path_prefix).as_str(), self.x);
        s.set(format!("{}.y"     , path_prefix).as_str(), self.y);
        s.set(format!("{}.width" , path_prefix).as_str(), self.width);
        s.set(format!("{}.height", path_prefix).as_str(), self.height);
    }
}

