// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT
pub mod lua;

use std::sync::{Arc, Mutex, Weak};

use std::collections::{HashMap, HashSet};

#[allow(unused_imports)]
use crate::logging::{debug, info, warn, error};

use crate::ui;
use crate::overlay;
use crate::input;

use crate::lua_manager;

use windows::Win32::Foundation;

pub struct Button {
    inner: Mutex<ButtonInner>,
}

struct ButtonInner {
    child: Option<Arc<ui::Element>>,

    x: i64,
    y: i64,

    width: i64,
    height: i64,

    min_width: i64,
    min_height: i64,

    is_checkbox: bool,

    toggle_state: bool,

    // colors
    bg: ui::Color,
    bg_hover: ui::Color,
    bg_highlight: ui::Color,
    border: ui::Color,

    border_width: i64,

    highlight: bool,
    hover: bool,

    last_scissor: Foundation::RECT,

    event_handlers: HashMap<i64, HashSet<String>>,

    ui: Weak<ui::Ui>,
}

impl Button {
    pub fn new() -> Arc<ui::Element> {
        let settings = crate::overlay::settings();
        
        let bg           = settings.get_color("overlay.ui.colors.buttonBG"         ).unwrap();
        let bg_hover     = settings.get_color("overlay.ui.colors.buttonBGHover"    ).unwrap();
        let bg_highlight = settings.get_color("overlay.ui.colors.buttonBGHighlight").unwrap();
        let border       = settings.get_color("overlay.ui.colors.buttonBorder"     ).unwrap();

        Arc::new(ui::Element::Button(Button{
            inner: Mutex::new(ButtonInner {
                child: None,

                x: 0,
                y: 0,

                width: 0,
                height: 0,

                min_width: 0,
                min_height: 0,

                is_checkbox: false,

                toggle_state: false,

                bg: bg,
                bg_hover: bg_hover,
                bg_highlight: bg_highlight,
                border: border,

                border_width: 1,

                highlight: false,
                hover: false,

                last_scissor: Foundation::RECT::default(),

                event_handlers: HashMap::new(),

                ui: Arc::downgrade(&overlay::ui()),
            })
        }))
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

    pub fn get_preferred_width(&self) -> i64 {
        let inner = self.inner.lock().unwrap();

        if let Some(c) = &inner.child {
            c.get_preferred_width() + (inner.border_width * 2)
        } else {
            inner.min_width
        }
    }

    pub fn get_preferred_height(&self) -> i64 {
        let inner = self.inner.lock().unwrap();

        if let Some(c) = &inner.child {
            c.get_preferred_height() + (inner.border_width * 2)
        } else {
            inner.min_height
        }
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
        self.inner.lock().unwrap().bg
    }

    pub fn set_bg_color(&self, bg: ui::Color) {
        self.inner.lock().unwrap().bg = bg;
    }

    pub fn on_lost_focus(&self) { }
}

impl ButtonInner {
     pub fn draw(
        &mut self,
        offset_x: i64,
        offset_y: i64,
        frame: &mut crate::dx::SwapChainLock,
        element: &Arc<ui::Element>,
    ) {
        let x = offset_x + self.x;
        let y = offset_y + self.y;
        let w = self.width;
        let h = self.height;
        let cw = w - (self.border_width * 2);
        let ch = h - (self.border_width * 2);
        let cx = x + self.border_width;
        let cy = y + self.border_width;

        if let Some(c) = &self.child {
            c.set_width(cw);
            c.set_height(ch);
        }

        let r = &self.ui.upgrade().unwrap().rect;
        //let r = &overlay::ui().rect;

        if self.border_width > 0 {
            r.draw(frame, x, y, w, self.border_width, self.border);                           // top
            r.draw(frame, x, y + h - self.border_width, w, self.border_width, self.border);  // bottom
            r.draw(frame, x, y, self.border_width, h, self.border);                           // left
            r.draw(frame, x + w - self.border_width, y, self.border_width, h, self.border); // right
        }

        let bg: ui::Color;
        
        if self.highlight { 
            bg = self.bg_highlight;
        } else if self.hover {
            bg = self.bg_hover;
        } else if self.toggle_state {
            bg = self.border;
        } else {
            bg = self.bg;
        }

        // background
        let bgw = w - (self.border_width * 2);
        let bgh = h - (self.border_width * 2);
        let bgx = x + self.border_width;
        let bgy = y + self.border_width;
        r.draw(frame, bgx, bgy, bgw, bgh, bg);

        self.last_scissor = frame.current_scissor();
        self.ui.upgrade().unwrap().add_input_element(element, offset_x, offset_y, self.last_scissor.clone());

        if let Some(c) = &self.child {
            c.draw(cx, cy, frame);
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
            input::MouseEvent::Enter(_) => {
                self.hover = true;
                self.queue_events("enter");
            },
            input::MouseEvent::Leave(_) => {
                self.hover = false;
                self.queue_events("leave");
            },
            input::MouseEvent::Button(btn) => {
                if btn.down {
                    self.highlight = true;
                    self.ui.upgrade().unwrap().set_mouse_capture(element, offset_x, offset_y, self.last_scissor.clone());
                } else {
                    let btn_x = self.x + offset_x;
                    let btn_y = self.y + offset_y;
                    let btn_w = self.width;
                    let btn_h = self.height;

                    self.highlight = false;
                    self.ui.upgrade().unwrap().clear_mouse_capture();

                    if btn.x >= btn_x && btn.x <= btn_x + btn_w &&
                       btn.y >= btn_y && btn.y <= btn_y + btn_h 
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

                        if self.is_checkbox {
                            self.toggle_state = !self.toggle_state;

                            let onoff = if self.toggle_state { "on" } else { "off" };
                            self.queue_events(format!("toggle-{}", onoff).as_str());
                        }
                    }
                }
            },
            _ => {
                // if we don't process the event then we don't consume it
                return false;
            },
        }

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
