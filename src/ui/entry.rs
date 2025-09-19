// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT
pub mod lua;

use std::sync::{Arc, Mutex, Weak};

use std::collections::{HashMap, HashSet};

#[allow(unused_imports)]
use crate::logging::{debug, info, warn, error};

use crate::ui;
use crate::input;
use crate::lua_manager;

use windows::Win32::UI::Input::KeyboardAndMouse;

pub struct Entry {
    inner: Mutex<EntryInner>,
}

struct EntryInner {
    text: String,
    hint: Option<String>,

    font: Arc<ui::font::Font>,

    pref_width: i64,
    pref_height: i64,

    x: i64,
    y: i64,
    width: i64,
    height: i64,

    readonly: bool,

    caret_pos: usize, // character pos
    caret_x: i64,   // pixel pos

    fg_color: ui::Color,
    hint_color: ui::Color,
    bg_color: ui::Color,
    border_color: ui::Color,
    border_focus_color: ui::Color,

    event_handlers: HashMap<i64, HashSet<String>>,

    ui: Weak<ui::Ui>,
}

impl Entry {
    pub fn new(font: &Arc<ui::font::Font>) -> Arc<ui::Element> {
        let settings = crate::overlay::settings();

        let i = EntryInner {
            text: String::new(),
            hint: None,

            font: font.clone(),

            pref_width: 50,
            pref_height: font.get_line_spacing() as i64 + 6,

            x: 0,
            y: 0,
            width: 0,
            height: 0,

            readonly: false,

            caret_pos: 0,
            caret_x: 0,

            fg_color: settings.get_color("overlay.ui.colors.text").unwrap(),
            hint_color: settings.get_color("overlay.ui.colors.entryHint").unwrap(),
            bg_color: settings.get_color("overlay.ui.colors.entryBG").unwrap(),
            border_color: settings.get_color("overlay.ui.colors.windowBorder").unwrap(),
            border_focus_color: settings.get_color("overlay.ui.colors.windowBorderHighlight").unwrap(),

            event_handlers: HashMap::new(),

            ui: Arc::downgrade(&crate::overlay::ui()),
        };

        Arc::new(ui::Element::Entry(Entry { inner: Mutex::new(i) }))
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

    pub fn process_keyboard_event(&self, event: &input::KeyboardEvent) -> bool {
        self.inner.lock().unwrap().process_keyboard_event(event)
    }

    pub fn get_preferred_width(&self) -> i64 {
        self.inner.lock().unwrap().pref_width
    }

    pub fn get_preferred_height(&self) -> i64 {
        self.inner.lock().unwrap().pref_height
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

    pub fn on_lost_focus(&self) {
        self.inner.lock().unwrap().queue_events("unfocus");
    }
}

impl EntryInner {
    pub fn draw(
        &mut self,
        offset_x: i64,
        offset_y: i64,
        frame: &mut crate::dx::SwapChainLock,
        element: &Arc<ui::Element>
    ) {
        let x = offset_x + self.x;
        let y = offset_y + self.y;
        let w = self.width;
        let h = self.height;

        let tx = x + 2;
        let ty = y + 3;
        let tw = self.width - 4;
        let th = self.height - 6;

        let r = &self.ui.upgrade().unwrap().rect;

        let is_focus = self.ui.upgrade().unwrap().element_is_focus(element);

        let border_color = if is_focus {
            self.border_focus_color
        } else {
            self.border_color
        };

        // borders
        r.draw(frame, x, y, w, 1, border_color);         // top
        r.draw(frame, x, y + h - 1, w, 1, border_color); // bottom
        r.draw(frame, x, y, 1, h, border_color);         // left
        r.draw(frame, x + w - 1, y, 1, h, border_color); // right

        // bg
        r.draw(frame, x + 1, y + 1, w - 2, h - 2, self.bg_color);

        if frame.push_scissor(tx, ty, tx + tw + 1, ty + th + 1) {
            if self.text.len() > 0 {
                self.font.render_text(frame, tx, ty, &self.text, self.fg_color);
            } else if self.text.len() == 0 && self.hint.is_some() {
                let h = self.hint.as_ref().unwrap();
                self.font.render_text(frame, tx, ty, h, self.hint_color);
            }
            frame.pop_scissor();
        }

        // cursor
        let cursor_x = self.caret_x + tx;

        if !self.readonly && is_focus {
            // draw a caret bar
            r.draw(frame, cursor_x, ty, 2, th, self.fg_color);
        }

        self.ui.upgrade().unwrap().add_input_element(element, offset_x, offset_y, frame.current_scissor());
    }

    pub fn process_mouse_event(
        &self,
        _offset_x: i64,
        _offset_y: i64,
        event: &input::MouseEvent,
        element: &Arc<ui::Element>
    ) -> bool {
        if let input::MouseEvent::Button(b) = event {
            if b.button == input::MouseButtonEventButton::Left &&
               b.down &&
               !self.readonly &&
               !self.ui.upgrade().unwrap().element_is_focus(element)
            {
                self.ui.upgrade().unwrap().set_focus_element(Some(element.clone()));

                self.queue_events("focus");
            }
        }

        match event {
            input::MouseEvent::Enter(_) => self.queue_events("enter"),
            input::MouseEvent::Leave(_) => self.queue_events("leave"),
            input::MouseEvent::Button(btn) => {
                if !btn.down {
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
            },
            input::MouseEvent::Wheel(wheel) => {
                let ename: &str = if wheel.horizontal && wheel.value > 0 {
                    "wheel-right"
                } else if wheel.horizontal {
                    "wheel-left"
                } else if wheel.value > 0 {
                    "wheel-up"
                } else {
                    "wheel-down"
                };

                for _ in 0..wheel.value.abs() {
                    self.queue_events(ename);
                }
            },
            _ => { return false; },
        }

        true
    }

    fn update_caret_x(&mut self) {
        if self.caret_pos > self.text.len() { self.caret_pos = self.text.len() }

        self.caret_x = self.font.get_text_width(&self.text[0..self.caret_pos]) as i64;
    }

    pub fn process_keyboard_event(&mut self, event: &input::KeyboardEvent) -> bool {
        if let Some(c) = &event.chars {
            if self.caret_pos == self.text.len() {
                self.text.push_str(&c);
            } else {
                self.text.insert_str(self.caret_pos, &c);
            }

            self.caret_pos += 1;
            self.update_caret_x();
        }

        if event.down && event.vkey == KeyboardAndMouse::VK_BACK && !event.alt {
            if self.text.len() > 0 && self.caret_pos > 0 {
                self.text.remove(self.caret_pos - 1);
                self.caret_pos -= 1;
                self.update_caret_x();
            }
        }

        if event.down && event.vkey == KeyboardAndMouse::VK_DELETE && !event.alt {
            if self.caret_pos < self.text.len() {
                self.text.remove(self.caret_pos);
            }
        }

        if event.down && event.vkey == KeyboardAndMouse::VK_LEFT {
            if self.text.len() > 0 && self.caret_pos > 0 {
                self.caret_pos -= 1;
                self.update_caret_x();
            }
        }

        if event.down && event.vkey == KeyboardAndMouse::VK_RIGHT {
            self.caret_pos += 1;
            self.update_caret_x();
        }

        self.queue_events(event.to_string().as_str());

        true
    }

    fn queue_events(&self, event: &str) {
        for (target, events) in &self.event_handlers {
            if events.contains(event) {
                lua_manager::queue_targeted_event(*target, Some(Box::new(String::from(event))));
            }
        }
    }
}
