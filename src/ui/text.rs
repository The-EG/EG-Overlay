// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT
pub mod lua;

use std::sync::Arc;
use std::sync::Mutex;

#[allow(unused_imports)]
use crate::logging::{debug, info, warn, error};

use crate::ui;
use crate::input;

pub struct Text {
    text: Mutex<TextInner>,
}

struct TextInner {
    text: String,
    line_breaks: Vec<usize>,
    font: Arc<ui::font::Font>,

    pref_width: i64,
    pref_height: i64,

    x: i64,
    y: i64,
    width: i64,
    height: i64,

    fg_color: ui::Color,
    bg_color: ui::Color,
}

impl Text {
    pub fn new(text: &str, color: ui::Color, font: &Arc<ui::font::Font>) -> Arc<ui::Element> {
        let mut t = TextInner {
            text: String::from(text.replace("\t","    ")),
            line_breaks: Vec::new(),
            font: font.clone(),

            pref_width: 0,
            pref_height: 0,

            x: 0,
            y: 0,
            width: 0,
            height: 0,

            fg_color: color,
            bg_color: ui::Color::from(0x00000000u32),
        };

        t.update_text_size();

        Arc::new(ui::Element::Text(Text{text:Mutex::new(t)}))
    }

    pub fn draw(
        &self,
        offset_x: i64,
        offset_y: i64,
        frame: &mut crate::dx::SwapChainLock,
        element: &Arc<ui::Element>
    ) {
        self.text.lock().unwrap().draw(offset_x, offset_y, frame, element);
    }

    pub fn process_mouse_event(
        &self,
        _offset_x: i64,
        _offset_y: i64,
        _event: &input::MouseEvent,
        _element: &Arc<ui::Element>
    ) -> bool {
        false
    }

    pub fn process_keyboard_event(&self, _event: &input::KeyboardEvent) -> bool {
        false
    }

    pub fn get_preferred_width(&self) -> i64 {
        self.text.lock().unwrap().pref_width
    }

    pub fn get_preferred_height(&self) -> i64 {
        self.text.lock().unwrap().pref_height
    }

    pub fn get_x(&self) -> i64 {
        self.text.lock().unwrap().x
    }

    pub fn set_x(&self, x: i64) {
        self.text.lock().unwrap().x = x;
    }

    pub fn get_y(&self) -> i64 {
        self.text.lock().unwrap().y
    }

    pub fn set_y(&self, y: i64) {
        self.text.lock().unwrap().y = y;
    }

    pub fn get_width(&self) -> i64 {
        self.text.lock().unwrap().width
    }

    pub fn set_width(&self, width: i64) {
        self.text.lock().unwrap().width = width;
    }

    pub fn get_height(&self) -> i64 {
        self.text.lock().unwrap().height
    }

    pub fn set_height(&self, height: i64) {
        self.text.lock().unwrap().height = height;
    }

    pub fn get_bg_color(&self) -> ui::Color {
        self.text.lock().unwrap().bg_color
    }

    pub fn set_bg_color(&self, color: ui::Color) {
        self.text.lock().unwrap().bg_color = color;
    }

    pub fn on_lost_focus(&self)  { }
}

impl TextInner {
    pub fn update_text_size(&mut self) {
        let mut cind = 0;

        self.line_breaks.clear();

        for c in self.text.chars() {
            if c == '\n' {
                self.line_breaks.push(cind);
            }
            cind += 1;
        }

        self.line_breaks.push(self.text.len());

        self.pref_width = 0;
        let mut prev_end: usize = 0;
        for &end in &self.line_breaks {
            let line = &self.text[prev_end..end];
            let w = self.font.get_text_width(line) as i64;
            if w > self.pref_width { self.pref_width = w; }
            prev_end = end + 1;
        }

        if self.line_breaks.len() > 0 {
            self.pref_height = self.font.get_line_spacing() as i64 * self.line_breaks.len() as i64;
        } else {
            self.pref_height = self.font.get_line_spacing() as i64;
        }
    }

    pub fn draw(
        &self,
        offset_x: i64,
        offset_y: i64,
        frame: &mut crate::dx::SwapChainLock,
        _element: &Arc<ui::Element>
    ) {
        let x = self.x + offset_x;
        let mut y = self.y + offset_y;

        let line_height = self.font.get_line_spacing() as i64;

        if frame.push_scissor(x, y, x + self.width + 1, y + self.height + 1) {
            let mut prev_end: usize = 0;
            for &end in &self.line_breaks {
                let line = &self.text[prev_end..end];

                self.font.render_text(frame, x, y, line, self.fg_color);
                y += line_height;

                prev_end = end + 1;
            }
            frame.pop_scissor();
        }
    }
}

