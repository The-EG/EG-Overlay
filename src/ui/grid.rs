// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT
pub mod lua;

use std::sync::{Arc, Mutex};

#[allow(unused_imports)]
use crate::logging::{debug, info, warn, error};

use crate::ui;
use crate::dx;
use crate::input;

//const LOG_TARGET: &str = "ui.grid";

pub struct Grid {
    inner: Mutex<GridInner>,
}

struct GridInner {
    x: i64,
    y: i64,

    width: i64,
    height: i64,

    rows: i64,
    cols: i64,

    cells: Vec<Option<GridItem>>,

    rowspacing: Vec<i64>,
    colspacing: Vec<i64>,

    rowheights: Vec<i64>,
    colwidths : Vec<i64>,
}

pub struct GridItem {
    element: Arc<ui::Element>,

    rowspan: i64,
    colspan: i64,

    halign: ui::ElementAlignment,
    valign: ui::ElementAlignment,
}

impl Grid {
    pub fn new(rows: i64, cols: i64) -> Arc<ui::Element> {
        let cell_count = (rows * cols) as usize;
        let mut cells: Vec<Option<GridItem>> = Vec::with_capacity(cell_count);

        cells.resize_with(cell_count, || None::<GridItem>);

        let g = GridInner {
            x: 0,
            y: 0,

            width: 0,
            height: 0,

            rows: rows,
            cols: cols,

            cells: cells,

            rowspacing: vec![0; (rows - 1) as usize],
            colspacing: vec![0; (cols - 1) as usize],

            rowheights: vec![0; rows as usize],
            colwidths : vec![0; cols as usize],
        };

        Arc::new(ui::Element::Grid(Grid{ inner: Mutex::new(g) }))
    }

    pub fn draw(
        &self,
        offset_x: i64,
        offset_y: i64,
        frame: &mut dx::SwapChainLock,
        element: &Arc<ui::Element>
    ) {
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

    pub fn get_preferred_width(&self) -> i64 {
        self.inner.lock().unwrap().get_preferred_width()
    }

    pub fn get_preferred_height(&self) -> i64 {
        self.inner.lock().unwrap().get_preferred_height()
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

impl GridInner {
    pub fn draw(
        &self,
        offset_x: i64,
        offset_y: i64,
        frame: &mut dx::SwapChainLock,
        _element: &Arc<ui::Element>
    ) {
        let mut cx: i64;// = offset_x + self.x;
        let mut cy = offset_y + self.y;

        for r in 0..self.rows {
            cx = offset_x + self.x;

            for c in 0..self.cols {
                if let Some(cell) = self.get_item(r, c) {
                    let mut itemw = cell.element.get_preferred_width();
                    let mut itemh = cell.element.get_preferred_height();

                    let mut extra_x = 0;
                    let mut extra_y = 0;

                    match cell.halign {
                        ui::ElementAlignment::Start => {},
                        ui::ElementAlignment::Middle => {
                            let mut totalwidth = 0;
                            for cs in 0..cell.colspan {
                                totalwidth += self.colwidths[(c + cs) as usize];
                                if cs > 0 && (c + cs -1) < self.cols -1 {
                                    totalwidth += self.colspacing[(c + cs - 1) as usize];
                                }
                            }

                            extra_x += ((totalwidth as f64 / 2.0) - (itemw as f64 / 2.0)) as i64;
                        },
                        ui::ElementAlignment::End => {
                            let mut totalwidth = 0;
                            for cs in 0..cell.colspan {
                                totalwidth += self.colwidths[(c + cs) as usize];
                                if cs > 0 && (c + cs -1) < self.cols -1 {
                                    totalwidth += self.colspacing[(c + cs - 1) as usize];
                                }
                            }

                            extra_x += totalwidth - itemw;
                        },
                        ui::ElementAlignment::Fill => {
                            let mut totalwidth = 0;
                            for cs in 0..cell.colspan {
                                totalwidth += self.colwidths[(c + cs) as usize];
                                if cs > 0 && (c + cs -1) < self.cols -1 {
                                    totalwidth += self.colspacing[(c + cs - 1) as usize];
                                }
                            }

                            itemw = totalwidth;
                        },
                    }

                    match cell.valign {
                        ui::ElementAlignment::Start => {},
                        ui::ElementAlignment::Middle => {
                            let mut totalheight = 0;
                            for rs in 0..cell.rowspan {
                                totalheight += self.rowheights[(r + rs) as usize];
                                if rs > 0 && (r + rs - 1) < self.rows - 1 {
                                    totalheight += self.rowspacing[(r + rs - 1) as usize];
                                }
                            }

                            extra_y += ((totalheight as f64 / 2.0) - (itemh as f64 / 2.0)) as i64;
                        },
                        ui::ElementAlignment::End => {
                            let mut totalheight = 0;
                            for rs in 0..cell.rowspan {
                                totalheight += self.rowheights[(r + rs) as usize];
                                if rs > 0 && (r + rs - 1) < self.rows - 1 {
                                    totalheight += self.rowspacing[(r + rs - 1) as usize];
                                }
                            }

                            extra_y += totalheight - itemh;
                        },
                        ui::ElementAlignment::Fill => {
                            let mut totalheight = 0;
                            for rs in 0..cell.rowspan {
                                totalheight += self.rowheights[(r + rs) as usize];
                                if rs > 0 && (r + rs - 1) < self.rows - 1 {
                                    totalheight += self.rowspacing[(r + rs - 1) as usize];
                                }
                            }

                            itemh = totalheight;
                        },
                    }

                    if itemw > 0 {
                        cell.element.set_width(itemw);
                    }
                    if itemh > 0 {
                        cell.element.set_height(itemh);
                    }

                    cell.element.draw(cx + extra_x, cy + extra_y, frame);
                }

                cx += self.colwidths[c as usize];

                if c < (self.cols-1) {
                    cx += self.colspacing[c as usize];
                }
            }

            cy += self.rowheights[r as usize];
            if r < (self.rows-1) {
                cy += self.rowspacing[r as usize];
            }
        }
    }

    pub fn get_preferred_width(&mut self) -> i64 {
        let mut w: i64 = 0;

        for c in 0..(self.cols as usize) {
            self.colwidths[c] = 0;
        }

        for r in 0..self.rows {
            for c in 0..self.cols {
                let cell = self.get_item(r,c);

                if cell.is_none() { continue; }

                let i = cell.unwrap();

                let cw = i.element.get_preferred_width();

                if cw > self.colwidths[c as usize] && i.colspan==1 { self.colwidths[c as usize] = cw; }
            }
        }

        for c in 0..(self.cols as usize) {
            w += self.colwidths[c];
            if c < (self.cols - 1) as usize {
                w += self.colspacing[c];
            }
        }

        w
    }

    pub fn get_preferred_height(&mut self) -> i64 {
        let mut h: i64 = 0;

        for r in 0..(self.rows as usize) {
            self.rowheights[r] = 0;
        }

        for r in 0..self.rows {
            for c in 0..self.cols {
                let cell = self.get_item(r,c);

                if cell.is_none() { continue; }

                let i = cell.unwrap();

                let ch = i.element.get_preferred_height();

                if ch > self.rowheights[r as usize] && i.rowspan==1 { self.rowheights[r as usize] = ch; }
            }
        }

        for r in 0..(self.rows as usize) {
            h += self.rowheights[r];
            if r < (self.rows - 1) as usize {
                h += self.rowspacing[r];
            }
        }

        h
    }

    pub fn get_item<'a>(&'a self, row: i64, col: i64) -> Option<&'a GridItem> {
        let cell = ((row * self.cols) + col) as usize;

        self.cells[cell].as_ref()
    }

    pub fn set_item(&mut self, row: i64, col: i64, item: Option<GridItem>) {
        let cell = ((row * self.cols) + col) as usize;

        self.cells[cell] = item;
    }
}
