// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT
pub mod lua;

use std::sync::Arc;
use std::sync::Mutex;
use std::sync::Weak;

#[allow(unused_imports)]
use crate::logging::{debug, info, warn, error};

use std::collections::{HashMap, HashSet};

use crate::ui;
use crate::input;

pub struct Text {
    text: Mutex<TextInner>,
}

struct TextInner {
    text: String,
    font: Arc<ui::font::Font>,

    pref_width: i64,
    pref_height: i64,

    x: i64,
    y: i64,
    width: i64,
    height: i64,

    fg_color: ui::Color,
    bg_color: ui::Color,

    events: bool,

    event_handlers: HashMap<i64, HashSet<String>>,

    ui: Weak<ui::Ui>,
}

impl Text {
    pub fn new(text: &str, color: ui::Color, font: &Arc<ui::font::Font>) -> Arc<ui::Element> {
        let mut t = TextInner {
            text: String::from(text.replace("\t","    ")),
            font: font.clone(),

            pref_width: 0,
            pref_height: 0,

            x: 0,
            y: 0,
            width: 0,
            height: 0,

            fg_color: color,
            bg_color: ui::Color::from(0x00000000u32),

            events: false,

            event_handlers: HashMap::new(),

            ui: Arc::downgrade(&crate::overlay::ui()),
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
        offset_x: i64,
        offset_y: i64,
        event: &input::MouseEvent,
        element: &Arc<ui::Element>
    ) -> bool {
        self.text.lock().unwrap().process_mouse_event(offset_x, offset_y, event, element)
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
        let mut lines = 0;

        self.pref_width = 0;

        for line in self.text.lines() {
            lines += 1;

            let w = self.font.get_text_width(line) as i64;
            if w > self.pref_width { self.pref_width = w; }
        }

        self.pref_height = self.font.get_line_spacing() as i64 * lines;
    }

    pub fn draw(
        &self,
        offset_x: i64,
        offset_y: i64,
        frame: &mut crate::dx::SwapChainLock,
        element: &Arc<ui::Element>
    ) {
        let x = self.x + offset_x;
        let mut y = self.y + offset_y;

        let line_height = self.font.get_line_spacing() as i64;

        if frame.push_scissor(x, y, x + self.width + 1, y + self.height + 1) {
            for line in self.text.lines() {
                self.font.render_text(frame, x, y, line, self.fg_color);
                y += line_height;
            }

            if self.events {
                let ui = self.ui.upgrade().unwrap();
                ui.add_input_element(element, offset_x, offset_y, frame.current_scissor().clone());
            }

            frame.pop_scissor();
        }
    }

    pub fn process_mouse_event(
        &self,
        offset_x: i64,
        offset_y: i64,
        event: &input::MouseEvent,
        _element: &Arc<ui::Element>
    ) -> bool {
        match event {
            input::MouseEvent::Enter(_) => self.queue_events("enter"),
            input::MouseEvent::Leave(_) => self.queue_events("leave"),
            input::MouseEvent::Button(btn) => {
                if !btn.down {
                    let txt_x = offset_x + self.x;
                    let txt_y = offset_y + self.y;
                    if btn.x >= txt_x && btn.x <= txt_x + self.width &&
                       btn.y >= txt_y && btn.y <= txt_y + self.height
                    {
                        let btnnm: &str = match btn.button {
                            input::MouseButtonEventButton::Left    => "left",
                            input::MouseButtonEventButton::Right   => "right",
                            input::MouseButtonEventButton::Middle  => "middle",
                            input::MouseButtonEventButton::X1      => "x1",
                            input::MouseButtonEventButton::X2      => "x2",
                            input::MouseButtonEventButton::Unknown => "unk",
                        };

                        self.queue_events(format!("click-{}", btnnm).as_str());
                    }
                }
            },
            _ => return false,
        }

        true
    }

    pub fn queue_events(&self, event: &str) {
        for (target, events) in &self.event_handlers {
            if events.contains(event) {
                crate::lua_manager::queue_targeted_event(*target, Some(Box::new(String::from(event))));
            }
        }
    }
}

