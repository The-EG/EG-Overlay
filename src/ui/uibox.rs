// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT
pub mod lua;

use std::sync::Arc;
use std::sync::Mutex;

//#[allow(unused_imports)]
//use crate::logging::{debug, info, warn, error};

use std::collections::VecDeque;

use crate::ui;
use crate::input;

//const LOG_TARGET: &str = "ui.box";

pub struct Box {
    inner: Mutex<BoxInner>,
}

struct BoxItem {
    element: Arc<ui::Element>,
    expand: bool,
    alignment: ui::ElementAlignment,
}

struct BoxInner {
    x: i64,
    y: i64,

    width: i64,
    height: i64,

    orientation: ui::ElementOrientation,
    alignment: ui::ElementAlignment,

    items: VecDeque<BoxItem>,

    padding_left: i64,
    padding_right: i64,
    padding_top: i64,
    padding_bottom: i64,

    spacing: i64,

    //events: bool,
}

impl Box {
    pub fn new(orientation: ui::ElementOrientation) -> Arc<ui::Element> {
        Arc::new(ui::Element::Box(Box {
            inner: Mutex::new(BoxInner {
                x: 0,
                y: 0,

                width: 0,
                height: 0,

                orientation: orientation,
                alignment: ui::ElementAlignment::Start,

                items: VecDeque::new(),

                padding_left: 0,
                padding_right: 0,
                padding_top: 0,
                padding_bottom: 0,

                spacing: 0,

                //events: false,
            }),
        }))
    }

    pub fn draw(&self, offset_x: i64, offset_y: i64, frame: &mut crate::dx::SwapChainLock, element: &Arc<ui::Element>) {
        self.inner.lock().unwrap().draw(offset_x, offset_y, frame, element);
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

    pub fn get_bg_color(&self) -> ui::Color { ui::Color::from(0x00000000u32) }
    pub fn set_bg_color(&self, _: ui::Color) {}

    pub fn get_preferred_width(&self) -> i64 {
        self.inner.lock().unwrap().get_preferred_width()
    }

    pub fn get_preferred_height(&self) -> i64 {
        self.inner.lock().unwrap().get_preferred_height()
    }

    pub fn push_front(&self, item: &Arc<ui::Element>, alignment: ui::ElementAlignment, expand: bool) {
        self.inner.lock().unwrap().items.push_front(BoxItem {
            element: item.clone(),
            alignment: alignment,
            expand: expand
        });
    }

    pub fn push_back(&self, item: &Arc<ui::Element>, alignment: ui::ElementAlignment, expand: bool) {
        self.inner.lock().unwrap().items.push_back(BoxItem {
            element: item.clone(),
            alignment: alignment,
            expand: expand,
        });
    }

    pub fn pop_front(&self) {
        self.inner.lock().unwrap().items.pop_front();
    }

    pub fn pop_back(&self) {
        self.inner.lock().unwrap().items.pop_back();
    }

    pub fn remove_item(&self, item: &Arc<ui::Element>) {
        self.inner.lock().unwrap().items.retain(|x| !Arc::ptr_eq(&x.element, item));
    }

    pub fn insert_before(&self, before: &Arc<ui::Element>, item: &Arc<ui::Element>, alignment: ui::ElementAlignment, expand: bool) -> bool {
        let mut inner = self.inner.lock().unwrap();

        for i in 0..inner.items.len() {
            if Arc::ptr_eq(&before, &inner.items.get(i).unwrap().element) {
                inner.items.insert(i, BoxItem {
                    element: item.clone(),
                    alignment: alignment,
                    expand: expand,
                });
                return true;
            }
        }

        return false;
    }

    pub fn insert_after(&self, after: &Arc<ui::Element>, item: &Arc<ui::Element>, alignment: ui::ElementAlignment, expand: bool) -> bool {
        let mut inner = self.inner.lock().unwrap();

        for i in 0..inner.items.len() {
            if Arc::ptr_eq(&after, &inner.items.get(i).unwrap().element) {
                inner.items.insert(i+1, BoxItem {
                    element: item.clone(),
                    alignment: alignment,
                    expand: expand,
                });

                return true;
            }
        }

        return false;
    }

    pub fn clear(&self) {
        self.inner.lock().unwrap().items.clear();
    }
}

impl BoxInner {
    pub fn draw(&mut self, offset_x: i64, offset_y: i64, frame: &mut crate::dx::SwapChainLock, _element: &Arc<ui::Element>) {
        let mut pref_width = self.get_preferred_width();
        let mut pref_height = self.get_preferred_height();

        if self.width < pref_width { pref_width = self.width }
        if self.height < pref_height { pref_height = self.height }

        let mut extra_room = match self.orientation {
            ui::ElementOrientation::Horizontal => { self.width - pref_width },
            ui::ElementOrientation::Vertical   => { self.height - pref_height },
        };

        let mut fill_items = 0;

        for item in &self.items {
            if item.expand { fill_items += 1; }
        }

        let item_fill_size = if fill_items > 0 { extra_room / fill_items } else { 0 };

        if fill_items == 0 { extra_room  = 0; }

        let scissor_left = offset_x + self.x + self.padding_left;
        let scissor_right = offset_x + self.x + self.width - self.padding_right;
        let scissor_top = offset_y + self.y + self.padding_top;
        let scissor_bottom = offset_y + self.y + self.height - self.padding_bottom;
        if frame.push_scissor(scissor_left, scissor_top, scissor_right, scissor_bottom) {
            match self.orientation {
            ui::ElementOrientation::Vertical => {
                let mut y = match self.alignment {
                    ui::ElementAlignment::Start |
                    ui::ElementAlignment::Fill   => offset_y + self.y + self.padding_top,
                    ui::ElementAlignment::Middle => offset_y + self.y + (self.height / 2) - ((pref_height + extra_room) / 2),
                    ui::ElementAlignment::End    => offset_y + self.y - self.padding_bottom - pref_height,
                };

                for item in &self.items {
                    let mut item_width = item.element.get_preferred_width();
                    let mut item_height = item.element.get_preferred_height();

                    if item.expand {
                        item_height += item_fill_size;
                    }

                    if item.alignment == ui::ElementAlignment::Fill {
                        item_width = self.width - self.padding_left - self.padding_right;
                    }

                    item.element.set_width(item_width);
                    item.element.set_height(item_height);

                    if item.alignment == ui::ElementAlignment::Fill {
                        item.element.set_width(self.width - self.padding_left - self.padding_right);
                    }

                    let x = match item.alignment {
                    ui::ElementAlignment::Start |
                    ui::ElementAlignment::Fill    => offset_x + self.x + self.padding_left,
                    ui::ElementAlignment::Middle  => offset_x + self.x + (self.width / 2) - (item_width / 2),
                    ui::ElementAlignment::End     => offset_x + self.x + self.width - self.padding_right - item_width,
                    };

                    item.element.draw(x, y, frame);

                    y += item_height;
                    if !std::ptr::eq(item, self.items.back().unwrap()) { y += self.spacing; }
                }
            },
            ui::ElementOrientation::Horizontal => {
                let mut x = match self.alignment {
                    ui::ElementAlignment::Start |
                    ui::ElementAlignment::Fill    => offset_x + self.x + self.padding_left,
                    ui::ElementAlignment::Middle  => offset_x + self.x + self.padding_left + ((self.width - self.padding_left - self.padding_right) / 2) - ((pref_width - self.padding_left - self.padding_right) / 2),
                    ui::ElementAlignment::End     => offset_x + self.x + self.width - self.padding_right - pref_width,
                };

                for item in &self.items {
                    let mut item_width = item.element.get_preferred_width();
                    let item_height = item.element.get_preferred_height();

                    if item.expand {
                        item_width += item_fill_size;
                    }

                    item.element.set_width(item_width);
                    item.element.set_height(item_height);

                    if item.alignment == ui::ElementAlignment::Fill {
                        item.element.set_height(self.height - self.padding_top - self.padding_bottom);
                    }

                    let y = match item.alignment {
                    ui::ElementAlignment::Start |
                    ui::ElementAlignment::Fill    => offset_y + self.y + self.padding_top,
                    ui::ElementAlignment::Middle  => offset_y + self.y + (self.height / 2) - (item_height / 2),
                    ui::ElementAlignment::End     => offset_y + self.y + self.height - self.padding_bottom - item_height,
                    };

                    item.element.draw(x, y, frame);

                    x += item_width;
                    if !std::ptr::eq(item, self.items.back().unwrap()) { x += self.spacing; }
                }
            }
            }

            frame.pop_scissor();
        }
    }

    pub fn get_preferred_width(&self) -> i64 {
        let mut w = 0;

        match self.orientation {
            ui::ElementOrientation::Horizontal => {
                for item in &self.items {
                    w += item.element.get_preferred_width();
                    if !std::ptr::eq(item, self.items.back().unwrap()) {
                        w += self.spacing;
                    }
                }
            },
            ui::ElementOrientation::Vertical => {
                for item in &self.items {
                    let cw = item.element.get_preferred_width();
                    if cw > w { w = cw; }
                }
            }
        }

        w += self.padding_left + self.padding_right;

        w
    }

    pub fn get_preferred_height(&self) -> i64 {
        let mut h = 0;

        match self.orientation {
            ui::ElementOrientation::Horizontal => {
                for item in &self.items {
                    let ch = item.element.get_preferred_height();
                    if ch > h { h = ch; }
                }
            },
            ui::ElementOrientation::Vertical => {
                for item in &self.items {
                    h += item.element.get_preferred_height();
                    if !std::ptr::eq(item, self.items.back().unwrap()) {
                        h += self.spacing;
                    }
                }
            }
        }

        h += self.padding_top + self.padding_bottom;

        h
    }
}
