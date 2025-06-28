// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT

//! EG-Overlay internal development documentation.
#![windows_subsystem = "windows"]

mod logging;
mod overlay;
mod utils;
mod dx;
mod lamath;
mod settings;
mod lua;
mod lua_json;
mod lua_manager;
mod lua_sqlite3;
mod ui;
mod input;
mod ml;
mod ft;
mod web_request;
mod zip;

mod version;
mod githash;

fn main() {
    overlay::init();
    overlay::run();
    overlay::cleanup();
}
