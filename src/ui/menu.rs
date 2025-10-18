// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT
pub mod lua;

use std::sync::{Arc, Mutex, Weak};

use std::collections::{HashMap, HashSet};

use crate::ui;
use crate::input;
use crate::overlay;

pub struct Menu {
    inner: Mutex<MenuInner>,
}

struct MenuInner {
    x: i64,
    y: i64,
    width: i64,
    height: i64,

    bg_color: ui::Color,
    border_color: ui::Color,

    // a uibox
    itembox: Arc<ui::Element>,

    parent: Weak<ui::Element>,
    child: Option<Arc<ui::Element>>,

    ui: Weak<ui::Ui>,
}

pub struct MenuItem {
    inner: Mutex<MenuItemInner>,
}

struct MenuItemInner {
    x: i64,
    y: i64,
    width: i64,
    height: i64,

    bg_color: ui::Color,
    hover_color: ui::Color,

    enabled: bool,
    hover: bool,

    icon_size: i64,
    post_size: i64,

    icon_text: String,
    icon_color: ui::Color,

    parent_menu: Weak<ui::Element>,
    child_menu: Option<Arc<ui::Element>>,

    element: Option<Arc<ui::Element>>,

    event_handlers: HashMap<i64, HashSet<String>>,

    ui: Weak<ui::Ui>,
}

impl Menu {
    pub fn new() -> Arc<ui::Element> {
        let settings = crate::overlay::settings();

        let m = MenuInner {
            x: 0,
            y: 0,
            width: 0,
            height: 0,

            bg_color: ui::Color::from(settings.get_u64("overlay.ui.colors.menuBG").unwrap() as u32),
            border_color: ui::Color::from(settings.get_u64("overlay.ui.colors.menuBorder").unwrap() as u32),

            itembox: ui::uibox::Box::new(ui::ElementOrientation::Vertical),

            parent: Weak::new(),
            child: None,

            ui: Arc::downgrade(&overlay::ui()),
        };

        Arc::new(ui::Element::Menu(Menu { inner: Mutex::new(m) }))
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
        self.inner.lock().unwrap().itembox.get_preferred_width() + 2
    }

    pub fn get_preferred_height(&self) -> i64 {
        self.inner.lock().unwrap().itembox.get_preferred_height() + 2
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

    pub fn set_bg_color(&self, color: ui::Color) {
        self.inner.lock().unwrap().bg_color = color;
    }

    pub fn show(&self, x: i64, y: i64, element: &Arc<ui::Element>) {
        let mut inner = self.inner.lock().unwrap();

        inner.x = x;
        inner.y = y;

        inner.child = None;

        let ui = inner.ui.upgrade().unwrap();
        ui.add_top_level_element(element);

        let (width, height) = ui.get_last_ui_size();
        let r = windows::Win32::Foundation::RECT {
            left: 0,
            right: width as i32,
            top: 0,
            bottom: height as i32,
        };
        ui.set_mouse_capture(element, 0, 0, r);
    }

    pub fn hide(&self, element: &Arc<ui::Element>) {
        let inner = self.inner.lock().unwrap();

        let ui = inner.ui.upgrade().unwrap();

        ui.clear_mouse_capture();
        ui.remove_top_level_element(element);
    }

    pub fn on_lost_focus(&self) { }
}

impl MenuInner {
    fn draw(
        &mut self,
        offset_x: i64,
        offset_y: i64,
        frame: &mut crate::dx::SwapChainLock,
        element: &Arc<ui::Element>
    ) {
        let boxw = self.itembox.get_preferred_width();
        let boxh = self.itembox.get_preferred_height();

        self.itembox.set_width(boxw);
        self.itembox.set_height(boxh);
        self.itembox.set_x(1);
        self.itembox.set_y(1);

        self.width = boxw + 2;
        self.height = boxh + 2;

        let r = &self.ui.upgrade().unwrap().rect;

        // background
        r.draw(frame, offset_x + self.x, offset_y + self.y, self.width, self.height, self.bg_color);

        //borders
        r.draw(frame, offset_x + self.x, offset_y + self.y, 1, self.height, self.border_color);                  // left
        r.draw(frame, offset_x + self.x + self.width - 1, offset_y + self.y, 1, self.height, self.border_color); // right
        r.draw(frame, offset_x + self.x, offset_y + self.y, self.width, 1, self.border_color);                   // top
        r.draw(frame, offset_x + self.x, offset_y + self.y + self.height - 1, self.width, 1, self.border_color); // bottom

        if self.parent.upgrade().is_none() {
            self.ui.upgrade().unwrap().add_input_element(element, offset_x, offset_y, frame.current_scissor());
        }

        self.itembox.draw(offset_x + self.x, offset_y + self.y, frame);

        if let Some(childmenu) = &self.child {
            childmenu.draw(0, 0, frame);
        }
    }

    pub fn process_mouse_event(
        &self,
        offset_x: i64,
        offset_y: i64,
        event: &input::MouseEvent,
        element: &Arc<ui::Element>
    ) -> bool {
        // We are only interested in btn down events, those will close the menu
        // if they occur outside of this menu or it's children. All other events
        // are handled by the menu items.
        if let input::MouseEvent::Button(btn) = event {
            if !btn.down { return false; }
        } else {
            return false;
        }

        // At this point event must be a button down event

        // This gets complicated due to child/sub menus.
        // When a menu is shown it captures mouse input so that it receives all
        // events first.
        // If a menu returns false from this function during that captured event, the
        // event will be sent to everything else that would normally get it, IE the
        // menu items, etc.
        // Therefore, the only time this function should really do anything is when
        // there is a mouse click outside of the menu or any child menu. Since a
        // child menu can also have a child menu, and so on, this can't be done
        // without recursion.
        let ex = event.x();
        let ey = event.y();

        let menu_left = offset_x + self.x;
        let menu_right = menu_left + self.width;
        let menu_top = offset_y + self.y;
        let menu_bottom = menu_top + self.height;

        let over_menu = ex >= menu_left && ex <= menu_right && ey >= menu_top && ey <= menu_bottom;

        // If this is a child menu and the mouse is over it, return true now.
        // This won't actually stop the event from reaching menu items because
        // of the next statements, which occur on the top level menu.
        if self.parent.upgrade().is_some() && over_menu { return true; }

        // If this is a child menu that also has a child menu and the event was
        // not within this menu, see if it was within the child menu
        if self.parent.upgrade().is_some() && self.child.is_some() {
            return self.child.as_ref().unwrap().process_mouse_event(offset_x, offset_y, event);
        }

        // If this is the top level menu and one of the child menus returned true
        // above, then return false here. This stops the click from being
        // interpreted as outside of the menu and lets it continue on to the
        // menu items, because it was within a child menu.
        if self.parent.upgrade().is_none() && self.child.is_some() {
            if self.child.as_ref().unwrap().process_mouse_event(offset_x, offset_y, event) {
                return false;
            }
        }

        // At this point the click must have been outside of this menu or any
        // of it's children. Close it.
        if !over_menu && self.parent.upgrade().is_none() {
            let ui = self.ui.upgrade().unwrap();
            ui.clear_mouse_capture();
            ui.remove_top_level_element(element);
        }

        false
    }
}

impl MenuItem {
    pub fn new() -> Arc<ui::Element> {
        let settings = crate::overlay::settings();

        let mi = MenuItemInner {
            x: 0,
            y: 0,
            width: 0,
            height: 0,

            bg_color: ui::Color::from(0x00000000u32),
            hover_color: ui::Color::from(settings.get_u64("overlay.ui.colors.menuItemHover").unwrap() as u32),

            enabled: true,
            hover: false,

            icon_size: overlay::ui().icon_font.get_line_spacing() as i64,
            post_size: 20,

            icon_text: String::new(),
            icon_color: ui::Color::from(0xFFFFFFFFu32),

            parent_menu: Weak::new(),
            child_menu: None,

            element: None,

            event_handlers: HashMap::new(),

            ui: Arc::downgrade(&overlay::ui()),
        };

        Arc::new(ui::Element::MenuItem(MenuItem { inner: Mutex::new(mi) }))
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

        let elew = if let Some(ele) = inner.element.as_ref() {
            ele.get_preferred_width()
        } else {
            0
        };

        elew + 10 + inner.icon_size + inner.post_size
    }

    pub fn get_preferred_height(&self) -> i64 {
        let inner = self.inner.lock().unwrap();

        if let Some(ele) = inner.element.as_ref() {
            let eh = ele.get_preferred_height() + 4;
            if eh > inner.icon_size || (inner.element.is_some() && inner.element.as_ref().unwrap().as_separator().is_some()) {
                return eh;
            } else {
                return inner.icon_size + 4;
            }
        } else {
            return inner.icon_size + 4;
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
        self.inner.lock().unwrap().bg_color
    }

    pub fn set_bg_color(&self, color: ui::Color) {
        self.inner.lock().unwrap().bg_color = color;
    }

    pub fn on_lost_focus(&self) { }
}

impl MenuItemInner {
    fn draw(
        &mut self,
        offset_x: i64,
        offset_y: i64,
        frame: &mut crate::dx::SwapChainLock,
        element: &Arc<ui::Element>
    ) {
        if self.element.is_none() { return; }

        let ele = self.element.as_ref().unwrap();

        //let elew = ele.get_preferred_width();
        let eleh = ele.get_preferred_height();

        ele.set_height(eleh);
        ele.set_width(self.width - 10 - self.icon_size - self.post_size);

        if frame.push_scissor(
            offset_x + self.x,
            offset_y + self.y,
            offset_x + self.x + self.width,
            offset_y + self.y + self.height
        ) {
            if self.hover {
                let r = &self.ui.upgrade().unwrap().rect;

                r.draw(frame, offset_x + self.x, offset_y + self.y, self.width, self.height, self.hover_color);
            }

            self.ui.upgrade().unwrap().add_input_element(element, offset_x, offset_y, frame.current_scissor());

            if self.icon_text.len() > 0 {
                let icon_x = self.x + offset_x + 5;
                let icon_y = self.y + offset_y + 2;
                self.ui.upgrade().unwrap().icon_font.render_text(
                    frame,
                    icon_x,
                    icon_y,
                    &self.icon_text,
                    self.icon_color,
                );
            }

            let ele_y = offset_y + self.y + ((self.height as f64 / 2.0) as i64 - (eleh as f64 / 2.0) as i64);

            ele.draw(offset_x + self.x + 5 + self.icon_size, ele_y, frame);

            if self.child_menu.is_some() {
                let post_x = self.x + offset_x + self.width - self.post_size;
                let post_y = self.y + offset_y + 2;
                self.ui.upgrade().unwrap().icon_font.render_text(
                    frame,
                    post_x,
                    post_y,
                    "\u{e5cc}",
                    ui::Color::from(0xFFFFFFFFu32)
                );
            }
            frame.pop_scissor();
        }
    }

    pub fn process_mouse_event(
        &mut self,
        offset_x: i64,
        offset_y: i64,
        event: &input::MouseEvent,
        _element: &Arc<ui::Element>
    ) -> bool {
        if !self.enabled { return true; }

        match event {
            input::MouseEvent::Enter(_) => {
                self.hover = true;
                self.queue_events("enter");

                if let Some(child) = self.child_menu.as_ref() {
                    let parent_menu = self.parent_menu.upgrade().unwrap();
                    parent_menu.as_menu().unwrap().inner.lock().unwrap().child = Some(child.clone());
                    child.set_x(offset_x + self.x + self.width - 10);
                    child.set_y(offset_y + self.y);
                    child.as_menu().unwrap().inner.lock().unwrap().parent = self.parent_menu.clone();
                    child.as_menu().unwrap().inner.lock().unwrap().child = None;
                } else {
                    self.parent_menu.upgrade().unwrap().as_menu().unwrap().inner.lock().unwrap().child = None;
                }
            },
            input::MouseEvent::Leave(_) => {
                self.hover = false;
                self.queue_events("leave");
            },
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
            _ => {},
        }

        true
    }

    fn queue_events(&self, event: &str) {
        for (target, events) in &self.event_handlers {
            if events.contains(event) {
                crate::lua_manager::queue_targeted_event(*target, Some(Box::new(String::from(event))));
            }
        }
    }
}
