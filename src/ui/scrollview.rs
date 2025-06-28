// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT
pub mod lua;

use std::sync::{Arc, Mutex, Weak};

#[allow(unused_imports)]
use crate::logging::{debug, info, warn, error};

use crate::ui;
use crate::overlay;
use crate::input;

pub struct ScrollView {
    inner: Mutex<ScrollViewInner>,
}

struct ScrollViewInner {
    child: Option<Arc<ui::Element>>,

    x: i64,
    y: i64,

    width: i64,
    height: i64,

    disp_x: i64,
    disp_y: i64,

    bg_color: ui::Color,

    scroll_thumb_color: ui::Color,
    scroll_thumb_hover_color: ui::Color,
    scroll_bg: ui::Color,

    child_height: i64,
    child_width: i64,

    last_drag_x: i64,
    last_drag_y: i64,

    drag_vert: bool,
    vert_hover: bool,
    vert_thumb_size: i64,
    vert_thumb_pos: i64,
    vert_move_factor: f64,

    drag_horiz: bool,
    horiz_hover: bool,
    horiz_thumb_size: i64,
    horiz_thumb_pos: i64,
    horiz_move_factor: f64,

    ui: Weak<ui::Ui>,
}

impl ScrollView {
    pub fn new() -> Arc<ui::Element> {
        let settings = overlay::settings();

        let i = ScrollViewInner {
            child: None,
            x: 0,
            y: 0,

            width: 0,
            height: 0,

            disp_x: 0,
            disp_y: 0,

            bg_color: ui::Color::from(0x00000000u32),

            scroll_thumb_color: settings.get_color("overlay.ui.colors.scrollThumb").unwrap(),
            scroll_thumb_hover_color: settings.get_color("overlay.ui.colors.scrollThumbHighlight").unwrap(),
            scroll_bg: settings.get_color("overlay.ui.colors.scrollBG").unwrap(),

            child_height: 0,
            child_width: 0,

            last_drag_x: 0,
            last_drag_y: 0,

            drag_vert: false,
            vert_hover: false,
            vert_thumb_size: 0,
            vert_thumb_pos: 0,
            vert_move_factor: 0.0,

            drag_horiz: false,
            horiz_hover: false,
            horiz_thumb_size: 0,
            horiz_thumb_pos: 0,
            horiz_move_factor: 0.0,

            ui: Arc::downgrade(&overlay::ui()),
        };

        Arc::new(ui::Element::ScrollView(ScrollView { inner: Mutex::new(i) }))
    }

    pub fn draw(
        &self,
        offset_x: i64,
        offset_y: i64,
        frame: &mut crate::dx::SwapChainLock,
        element: &Arc<ui::Element>
    ) {
        self.inner.lock().unwrap().draw(offset_x, offset_y, frame, element);
    }

    pub fn process_mouse_event(
        &self,
        offset_x: i64,
        offset_y: i64,
        event: &input::MouseEvent,
        element: &Arc<ui::Element>
    ) -> bool {
        self.inner.lock().unwrap().process_mouse_event(offset_x, offset_y, event, element)
    }

    pub fn process_keyboard_event(&self, _event: &input::KeyboardEvent) -> bool {
        false
    }

    pub fn get_x(&self) -> i64 {
        self.inner.lock().unwrap().x
    }

    pub fn set_x(&self, x: i64) {
        self.inner.lock().unwrap().x = x;
    }

    pub fn get_y(&self) -> i64 {
        self.inner.lock().unwrap().y
    }

    pub fn set_y(&self, y: i64) {
        self.inner.lock().unwrap().y = y;
    }

    pub fn get_preferred_width(&self) -> i64 {
        /*
        if let Some(c) = self.inner.lock().unwrap().child.as_ref() {
            c.get_preferred_width()
        } else {
            0
        }
        */
        0
    }

    pub fn get_preferred_height(&self) -> i64 {
        /*
        if let Some(c) = self.inner.lock().unwrap().child.as_ref() {
            c.get_preferred_height()
        } else {
            0
        }
        */
        0
    }

    pub fn get_width(&self) -> i64 {
        self.inner.lock().unwrap().width
    }

    pub fn set_width(&self, width: i64) {
        self.inner.lock().unwrap().width = width;
    }

    pub fn get_height(&self) -> i64 {
        self.inner.lock().unwrap().height
    }

    pub fn set_height(&self, height: i64) {
        self.inner.lock().unwrap().height = height;
    }

    pub fn get_bg_color(&self) -> ui::Color {
        self.inner.lock().unwrap().bg_color
    }

    pub fn set_bg_color(&self, bg: ui::Color) {
        self.inner.lock().unwrap().bg_color = bg;
    }
}

impl ScrollViewInner {
    pub fn draw(
        &mut self,
        offset_x: i64,
        offset_y: i64,
        frame: &mut crate::dx::SwapChainLock,
        element: &Arc<ui::Element>
    ) {
        let sx = self.x + offset_x;
        let sy = self.y + offset_y;

        if let Some(child) = self.child.as_ref() {
            self.ui.upgrade().unwrap().add_input_element(element, offset_x, offset_y);

            self.child_width = child.get_preferred_width();
            self.child_height = child.get_preferred_height();

            // the maximum valid disp_y
            let max_disp_y = self.child_height - self.height + 10;
            let max_disp_x = self.child_width - self.width + 10;

            if frame.push_scissor(sx, sy, sx + self.width - 10, sy + self.height - 10) {

                child.set_width(self.child_width);
                child.set_height(self.child_height);
                
                if self.disp_x < 0 { self.disp_x = 0; }

                if self.child_width > self.width - 10 {
                    if self.disp_x > max_disp_x {
                        self.disp_x = max_disp_x;
                    }
                } else if self.width - 10 >= self.child_width {
                    self.disp_x = 0;
                }

                if self.disp_y < 0 { self.disp_y = 0; }

                if self.child_height > self.height - 10 {
                    if self.disp_y > max_disp_y {
                        self.disp_y = max_disp_y
                    }
                } else if self.height - 10 >= self.child_height {
                    self.disp_y = 0;
                }

                let cx = sx - self.disp_x;
                let cy = sy - self.disp_y;

                child.draw(cx, cy, frame);

                frame.pop_scissor();
            }
            // scroll bars
            
            let r = &self.ui.upgrade().unwrap().rect;

            // scroll bar gutters/background
            r.draw(frame, sx + self.width - 10, sy, 10, self.height - 10, self.scroll_bg);
            r.draw(frame, sx, sy + self.height - 10, self.width - 10, 10, self.scroll_bg);

            if self.child_height > self.height - 10 {
                // how much of the child can this scrollview see vertically at any
                // give time?
                let mut vert_size_factor = (self.height - 10) as f64 / (self.child_height as f64);
                if vert_size_factor > 1.0 { vert_size_factor = 1.0; }

                // the size of the 'thumb' (scroll bar) is this same ratio
                self.vert_thumb_size = (vert_size_factor * (self.height - 10) as f64) as i64;

                // the total amount of travel the scroll bar has
                let vert_max_pos = self.height - 10 - self.vert_thumb_size;

                // finally the position (of the top) of the scroll bar
                self.vert_thumb_pos = (((self.disp_y as f64) / (max_disp_y as f64)) * vert_max_pos as f64) as i64;

                // the ratio between how much the scroll thumb moves and how much the view moves
                // this is used when dragging the scroll thumb with the mouse
                self.vert_move_factor = max_disp_y as f64 / vert_max_pos as f64;

                let vert_color = if self.vert_hover { self.scroll_thumb_hover_color }
                                 else               { self.scroll_thumb_color       };
   
                r.draw(frame, sx + self.width - 10, sy + self.vert_thumb_pos, 10, self.vert_thumb_size, vert_color);
            }

            if self.child_width > self.width - 10 {
                let mut horiz_size_factor = (self.width - 10) as f64 / (self.child_width as f64);
                if horiz_size_factor > 1.0 { horiz_size_factor = 1.0; }

                self.horiz_thumb_size = (horiz_size_factor * (self.width - 10) as f64) as i64;

                let horiz_max_pos = self.width - 10 - self.horiz_thumb_size;

                self.horiz_thumb_pos = (((self.disp_x as f64) / (max_disp_x as f64)) * horiz_max_pos as f64) as i64;

                self.horiz_move_factor = max_disp_x as f64 / horiz_max_pos as f64;

                let horiz_color = if self.horiz_hover { self.scroll_thumb_hover_color }
                                  else                { self.scroll_thumb_color       };

                r.draw(frame, sx + self.horiz_thumb_pos, sy + self.height - 10, self.horiz_thumb_size, 10, horiz_color);
            }
        }
    }

    pub fn process_mouse_event(
        &mut self,
        offset_x: i64,
        offset_y: i64,
        event: &input::MouseEvent,
        element: &Arc<ui::Element>
    ) -> bool {
        match &event {
            input::MouseEvent::Wheel(w)  => self.process_mouse_wheel(w),
            input::MouseEvent::Move(m)   => self.process_mouse_move(offset_x, offset_y, m),
            input::MouseEvent::Button(b) => self.process_mouse_button(offset_x, offset_y, b, element),
            input::MouseEvent::Leave(_)  => self.process_mouse_leave(),
            _ => false,
        }
    }

    fn process_mouse_wheel(&mut self, wheel: &input::MouseWheelEvent) -> bool {
        if !wheel.horizontal {
            self.disp_y -= wheel.value as i64 * 20;
        } else {
            self.disp_x += wheel.value as i64 * 20;
        }

        true
    }

    fn process_mouse_move(
        &mut self,
        offset_x: i64,
        offset_y: i64,
        move_: &input::MouseGenericEvent,
    ) -> bool {
        if self.child.is_some() {
            if self.drag_vert {
                let delta = move_.y - self.last_drag_y;
                self.disp_y += (delta as f64 * self.vert_move_factor) as i64;

                self.last_drag_x = move_.x;
                self.last_drag_y = move_.y;
            } else if self.drag_horiz {
                let delta = move_.x - self.last_drag_x;

                self.disp_x += (delta as f64 * self.horiz_move_factor) as i64;

                self.last_drag_x = move_.x;
                self.last_drag_y = move_.y;
            } else {
                self.hover_vert_scroll(offset_x, offset_y, move_.x, move_.y);
                self.hover_horiz_scroll(offset_x, offset_y, move_.x, move_.y);
            }
        }

        true
    }

    fn process_mouse_button(
        &mut self,
        offset_x: i64,
        offset_y: i64,
        button: &input::MouseButtonEvent,
        element: &Arc<ui::Element>
    ) -> bool {
        if !self.drag_vert && !self.drag_horiz && button.button==input::MouseButtonEventButton::Left && button.down {
            if self.vert_hover || self.horiz_hover {
                self.drag_vert = self.vert_hover;
                self.drag_horiz = self.horiz_hover;
                self.last_drag_x = button.x;
                self.last_drag_y = button.y;

                self.ui.upgrade().unwrap().set_mouse_capture(element, offset_x, offset_y);

                return true;
            }
        }

        if (self.drag_vert || self.drag_horiz) &&
           button.button==input::MouseButtonEventButton::Left && !button.down
        {
            self.drag_vert = false;
            self.drag_horiz = false;
            self.hover_vert_scroll(offset_x, offset_y, button.x, button.y);
            self.hover_horiz_scroll(offset_x, offset_y, button.x, button.y);
            self.ui.upgrade().unwrap().clear_mouse_capture();
            return true;
        }

        false
    }

    fn process_mouse_leave(&mut self) -> bool {
        if !self.drag_vert { self.vert_hover = false; }
        if !self.drag_horiz { self.horiz_hover = false; }

        true
    }

    fn hover_vert_scroll(&mut self, offset_x: i64, offset_y: i64, mx: i64, my: i64) {
        let vert_left = self.x + offset_x + self.width - 10;
        let vert_right = vert_left + 10;
        let vert_top = self.y + offset_y + self.vert_thumb_pos;
        let vert_bottom = vert_top + self.vert_thumb_size;

        self.vert_hover = mx >= vert_left && mx <= vert_right &&
                          my >= vert_top  && my <= vert_bottom;
    }

    fn hover_horiz_scroll(&mut self, offset_x: i64, offset_y: i64, mx: i64, my: i64) {
        let horiz_left = self.x + offset_x + self.horiz_thumb_pos;
        let horiz_right = horiz_left + self.horiz_thumb_size;
        let horiz_top = self.y + offset_y + self.height - 10;
        let horiz_bottom = horiz_top + 10;

        self.horiz_hover = mx >= horiz_left && mx <= horiz_right &&
                           my >= horiz_top  && my <= horiz_bottom;
    }
}

