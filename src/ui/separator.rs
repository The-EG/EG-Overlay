// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT
pub mod lua;

use std::sync::{Arc, Mutex, Weak};

use crate::ui;
use crate::input;
use crate::overlay;

pub struct Separator {
    inner: Mutex<SeparatorInner>,
}

struct SeparatorInner {
    x: i64,
    y: i64,

    orientation: ui::ElementOrientation,

    width: i64,
    height: i64,

    color: ui::Color,
    thickness: i64,

    ui: Weak<ui::Ui>,
}

impl Separator {
    pub fn new(orientation: ui::ElementOrientation) -> Arc<ui::Element> {
        let settings = crate::overlay::settings();

        let c = ui::Color::from(settings.get_u64("overlay.ui.colors.windowBorder").unwrap() as u32);

        let s = SeparatorInner {
            x: 0,
            y: 0,
            orientation: orientation,
            width: 0,
            height: 0,

            color: c,
            thickness: 1,

            ui: Arc::downgrade(&overlay::ui()),
        };

        Arc::new(ui::Element::Separator(Separator { inner: Mutex::new(s) }))
    }


    pub fn draw(
        &self,
        offset_x: i64,
        offset_y: i64,
        frame: &mut crate::dx::SwapChainLock,
        _element: &Arc<ui::Element>
    ) {
        let inner = self.inner.lock().unwrap();

        let r = &inner.ui.upgrade().unwrap().rect;
        //let r = &overlay::ui().rect;

        let mut w = inner.width;
        let mut h = inner.height;
        let mut x = offset_x + inner.x;
        let mut y = offset_y + inner.y;

        if w == 0 || h == 0 { return; }

        match inner.orientation {
            ui::ElementOrientation::Horizontal => {
                h -= 2;
                y += 1;
            },
            ui::ElementOrientation::Vertical   => {
                w -= 2;
                x += 1;
            },
        }

        r.draw(frame, x, y, w, h, inner.color);
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
        let inner = self.inner.lock().unwrap();
        
        match inner.orientation {
            ui::ElementOrientation::Horizontal => 20,
            ui::ElementOrientation::Vertical   => inner.thickness + 2,
        }
    }

    pub fn get_preferred_height(&self) -> i64 {
        let inner = self.inner.lock().unwrap();

        match inner.orientation {
            ui::ElementOrientation::Horizontal => inner.thickness + 2,
            ui::ElementOrientation::Vertical   => 20,
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

    pub fn get_bg_color(&self) -> ui::Color { todo!() }
    pub fn set_bg_color(&self, _color: ui::Color) { todo!() }

    pub fn on_lost_focus(&self) { }
}
