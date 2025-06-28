// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT

//! Overlay Lua Management

#[allow(unused_imports)]
use crate::logging::{debug, info, warn, error};

use crate::lua;

use std::collections::VecDeque;

use std::sync::Mutex;

use std::collections::{HashMap};


#[doc(hidden)]
static LUA_MANAGER: Mutex<Option<LuaManager>> = Mutex::new(None);

static LOG_MESSAGES: Mutex<VecDeque<String>> = Mutex::new(VecDeque::new());

static LUA_STATE: Mutex<Option<&lua::lua_State>> = Mutex::new(None);

/// The global Lua state.
struct LuaManager {
    module_openers: HashMap<String, lua::lua_CFunction>,

    events: VecDeque<LuaEvent>,
    targeted_events: VecDeque<TargetedLuaEvent>,
    event_handlers: HashMap<String, Vec<i64>>,
    keybind_handlers: HashMap<String, Vec<i64>>,

    coroutines: VecDeque<LuaCoRoutineThread>,
}

struct LuaCoRoutineThread {
    state: &'static lua::lua_State,
    thread_ref: i64,
}

struct LuaEvent {
    name: String,
    data: Option<Box<dyn ToLua + Sync + Send>>,
}

struct TargetedLuaEvent {
    target: i64,
    data: Option<Box<dyn ToLua + Sync + Send>>,
}

/// Searches for an internal module opener and returns pushes it to the stack if found
unsafe extern "C" fn embedded_module_searcher(l: &lua::lua_State) -> i32 {
    let name = unsafe { lua::L::checkstring(l, 1) };

    let luaman_lock = LUA_MANAGER.lock().unwrap();
    let luaman = luaman_lock.as_ref().unwrap();

    if luaman.module_openers.contains_key(&name) {
        let opener = luaman.module_openers.get(&name).unwrap();
        lua::pushcfunction(l, *opener);
        lua::pushstring(l, ":eg-overlay-embedded-module:");

        debug!("Loading embedded module {} @ {:p}", name, opener);

        return 2;
    }

    lua::pushnil(l);

    return 1;
}

/// Initializes the Lua state.
pub fn init() {
    info!("Initializing Lua...");

    let l = lua::L::newstate().expect("Couldn't initialize Lua.");

    lua::L::openlibs(l);

    // add our embedded module searcher
    match lua::getglobal(l, "package") {
        lua::LuaType::LUA_TTABLE => {},
        _ => { panic!("Couldn't get package table.") },
    }

    match lua::getfield(l, -1, "searchers") {
        lua::LuaType::LUA_TTABLE => {},
        _ => { panic!("Couldn't get package.searchers"); },
    }
    lua::pushcfunction(l, Some(embedded_module_searcher));

    // add it to the end of the table 'searchers'
    lua::seti(l, -2, (lua::L::len(l, -2) + 1) as i64);
    lua::pop(l, 2); // pop searchers and package

    let luaman = LuaManager {
        module_openers: HashMap::new(),

        events: VecDeque::new(),
        targeted_events: VecDeque::new(),
        event_handlers: HashMap::new(),
        keybind_handlers: HashMap::new(),
        coroutines: VecDeque::new(),
    };

    *LUA_MANAGER.lock().unwrap() = Some(luaman);
    *LUA_STATE.lock().unwrap() = Some(l);
}

/// Shuts down and cleans up the Lua state.
pub fn cleanup() {
    let mut state = LUA_STATE.lock().unwrap();

    let l = state.unwrap();
    debug!("Closing main Lua thread...");
    lua::close(l);
    
    *LUA_MANAGER.lock().unwrap() = None;
    *state = None;
}

/// Adds a module opener function.
///
/// `opener` will be called whenever a Lua module of `name` is attempted to be
/// opened and can not be found.
///
/// The opener function should create a new Lua table and register functions
/// and other publicly accessible data to it.
pub fn add_module_opener(name: &str, opener: lua::lua_CFunction) {
    let mut luaman = LUA_MANAGER.lock().unwrap();
    (*luaman).as_mut().unwrap().module_openers.insert(String::from(name), opener);
}

/// Runs the file at `path` as a Lua script with the Overlay's Lua state.
///
/// This is typically used for running an initial 'autoload.lua' script.
pub fn run_file(path: &str) {
    let l = LUA_STATE.lock().unwrap().unwrap();

    let thread = lua::newthread(l).expect("Couldn't create Lua thread.");

    if let Err(_r) = lua::L::loadfile(thread, path) {
        let err_msg = lua::tostring(thread, -1);
        panic!("Couldn't load {}: {}", path, err_msg);
    }

    let mut nres = 0i32;

    let mut r: i32;
    loop {
        r=lua::resume(thread, None, 0, &mut nres);

        if r!=lua::LUA_YIELD { break; }
        
        while resume_coroutines() { }
        run_event_queue();
    }

    if r!=lua::LUA_OK {
        let err_msg = lua::tostring(thread, -1);
        lua::L::traceback(l, thread, Some(&err_msg), 0);
        let traceback = lua::tostring(l, -1);
        error!("Error occured during lua run file: {}", traceback);

        lua::pop(l, 1);
        lua::pop(thread, 1);

        let _ = lua::closethread(thread, None);
    }
}

/// Something that can be pushed to Lua.
///
/// This might be something simple like a primitive value or complex like full
/// user data or a table of values.
pub trait ToLua {
    fn push_to_lua(&self, l: &lua::lua_State);
}

/// Adds an event handler from Lua.
pub fn add_lua_event_handler(event: &str, cbi: i64) {
    let mut lock = LUA_MANAGER.lock().unwrap();
    let lua = lock.as_mut().unwrap();

    if !lua.event_handlers.contains_key(event) {
        lua.event_handlers.insert(event.to_string(), Vec::new());
    }

    let handlers = lua.event_handlers.get_mut(event).unwrap();

    handlers.push(cbi);
}

/// Removes a Lua event handler.
pub fn remove_lua_event_handler(event: &str, cbi: i64) {
    let mut lock = LUA_MANAGER.lock().unwrap();
    let lua = lock.as_mut().unwrap();

    if !lua.event_handlers.contains_key(event) {
        warn!("{} has no event handlers.", event);
        return;
    }

    let handlers = lua.event_handlers.get_mut(event).unwrap();

    let mut i = 0;
    while i < handlers.len() {
        if handlers[i] == cbi {
            handlers.remove(i);
        } else {
            i += 1;
        }
    }
}

pub fn add_lua_keybind_handler(keybind: &str, cbi: i64) {
    let mut lock = LUA_MANAGER.lock().unwrap();
    let lua = lock.as_mut().unwrap();

    if !lua.keybind_handlers.contains_key(keybind) {
        lua.keybind_handlers.insert(keybind.to_string(), Vec::new());
    }

    let handlers = lua.keybind_handlers.get_mut(keybind).unwrap();

    handlers.push(cbi);
}

pub fn remove_lua_keybind_handler(keybind: &str, cbi: i64) {
    let mut lock = LUA_MANAGER.lock().unwrap();
    let lua = lock.as_mut().unwrap();

    let handlers = lua.keybind_handlers.get_mut(keybind).unwrap();

    let mut i = 0;
    while i < handlers.len() {
        if handlers[i] == cbi {
            handlers.remove(i);
        } else {
            i += 1;
        }
    }
}

/// Adds an event to be sent to Lua event handlers
pub fn queue_event(event: &str, data: Option<Box<dyn ToLua + Sync + Send>>) {
    let mut lock = LUA_MANAGER.lock().unwrap();
    let lua = lock.as_mut().unwrap();

    lua.events.push_back(LuaEvent {
        name: event.to_string(),
        data: data,
    });
}

/// Adds a targeted event to the event queue.
///
/// A targeted event is only sent to a specific function that registered for it.
/// Targeted events are basically async callbacks.
pub fn queue_targeted_event(target: i64, data: Option<Box<dyn ToLua + Sync + Send>>) {
    let mut lock = LUA_MANAGER.lock().unwrap();
    let lua = lock.as_mut().unwrap();

    lua.targeted_events.push_back(TargetedLuaEvent {
        target: target,
        data: data,
    });
}

pub fn process_keybinds(keyevent: &crate::input::KeyboardEvent) -> bool {
    if !keyevent.down { return false; }

    let l = LUA_STATE.lock().unwrap().unwrap();
    let keybinds = LUA_MANAGER.lock().unwrap().as_ref().unwrap().keybind_handlers.clone();

    let keyname = keyevent.full_name();

    if !keybinds.contains_key(&keyname) { return false; }

    for cb in keybinds.get(&keyname).unwrap() {
        lua::rawgeti(l, lua::LUA_REGISTRYINDEX, *cb);
        lua::pushstring(l, &keyname);

        match lua::pcall(l, 1, 1, 0) {
            Ok(_) => {
                let r = lua::toboolean(l, -1);
                lua::pop(l, 1);
                if r { return true; }
            },
            Err(_) => {
                let errmsg = lua::tostring(l, -1);
                error!("Error during keybind callback: {}", errmsg);
                lua::pop(l, 1);
            }
        }
    }
    
    false
}

pub fn run_event_queue() {
    // queue up events for log messages
    while let Some(msg) = LOG_MESSAGES.lock().unwrap().pop_front() {
        queue_event("log-message", Some(Box::new(String::from(msg))));
    }

    // first run the main event queue
    let mut lock = LUA_MANAGER.lock().unwrap();
    let luaman = lock.as_mut().unwrap();

    // empty the event queue now. any events added during the event handlers
    // will be added to the empty queue and will be dispatched the next time
    // this function is called.
    let mut events = luaman.events.drain(..).collect::<VecDeque<_>>();

    // take a copy of the current handlers and run those. handlers may add or
    // remove handlers while they are called
    let handlers = luaman.event_handlers.clone();

    // handlers may add event handlers, queue events, etc. so unlock the manager
    drop(lock);

    // but now we lock the lua thread
    let lua = LUA_STATE.lock().unwrap().unwrap();
    while let Some(event) = events.pop_front() {
        if !handlers.contains_key(&event.name) { continue; }

        for cbi in handlers.get(&event.name).unwrap() {
            let cothread = lua::newthread(lua).unwrap();

            // push the event handler function
            lua::rawgeti(cothread, lua::LUA_REGISTRYINDEX, *cbi);
            // the event name, first parameter
            lua::pushstring(cothread, &event.name);
            
            // the event data, second parameter
            if let Some(data) = &event.data {
                data.push_to_lua(cothread);
            } else {
                lua::pushnil(cothread);
            }

            let mut nres = 0;
            let status = lua::resume(cothread, None, 2, &mut nres);

            if status == lua::LUA_YIELD {
                // the event handler yielded, save the thread and resume it later
                if nres > 0 { lua::pop(cothread, nres); }

                let threadi = lua::L::ref_(lua, lua::LUA_REGISTRYINDEX);
                LUA_MANAGER.lock().unwrap().as_mut().unwrap().coroutines.push_back(LuaCoRoutineThread {
                    state: cothread,
                    thread_ref: threadi,
                });
            } else if status == lua::LUA_OK {
                // the handler returned normally, close the thread
                if nres > 0 { lua::pop(cothread, nres); }

                // pop the thread
                lua::pop(lua, 1);
                lua::closethread(cothread, None);
            } else {
                // error occurred in the handler
                let errmsg = lua::tostring(cothread, -1);
                lua::L::traceback(lua, cothread, Some(&errmsg), 0);
                let traceback = lua::tostring(lua, -1);

                error!("Error occured during lua event handler ({}): {}", event.name, traceback);
                lua::pop(lua, 1); // traceback
                lua::pop(cothread, 1); // errmsg
                lua::pop(lua, 1); // thread
                lua::closethread(cothread, None);
            }
        }
    }

    // then run targeted events
    let mut lock = LUA_MANAGER.lock().unwrap();
    let luaman = lock.as_mut().unwrap();

    let targeted_events = luaman.targeted_events.drain(..).collect::<VecDeque<_>>();
    
    // same as above, don't keep LuaManager locked, events might add event handlers, etc.
    drop(lock);

    for event in targeted_events {
        let cothread = lua::newthread(lua).unwrap();

        // push the event handler function
        lua::rawgeti(cothread, lua::LUA_REGISTRYINDEX, event.target);

        if lua::luatype(cothread, -1) != lua::LuaType::LUA_TFUNCTION {
            error!("Lua target is not a function.");

        }
        
        // the event data, first parameter
        if let Some(data) = &event.data {
            data.push_to_lua(cothread);
        } else {
            lua::pushnil(cothread);
        }

        let mut nres = 0;
        let status = lua::resume(cothread, None, 1, &mut nres);

        if status == lua::LUA_YIELD {
            // the event handler yielded, save the thread and resume it later
            if nres > 0 { lua::pop(cothread, nres); }

            let threadi = lua::L::ref_(lua, lua::LUA_REGISTRYINDEX);
            LUA_MANAGER.lock().unwrap().as_mut().unwrap().coroutines.push_back(LuaCoRoutineThread {
                state: cothread,
                thread_ref: threadi,
            });
        } else if status == lua::LUA_OK {
            // the handler returned normally, close the thread
            if nres > 0 { lua::pop(cothread, nres); }

            // pop the thread
            lua::pop(lua, 1);
            lua::closethread(cothread, None);
        } else {
            // error occurred in the handler
            let errmsg = lua::tostring(cothread, -1);
            lua::L::traceback(lua, cothread, Some(&errmsg), 0);
            let traceback = lua::tostring(lua, -1);

            error!("Error occured during targeted lua event handler ({}): {}", event.target, traceback);
            lua::pop(lua, 1); // traceback
            lua::pop(cothread, 1); // errmsg
            lua::pop(lua, 1); // thread
            lua::closethread(cothread, None);
        }
    }
}

pub fn resume_coroutines() -> bool {
    let mut lock = LUA_MANAGER.lock().unwrap();
    let luaman = lock.as_mut().unwrap();

    let mut coroutines = luaman.coroutines.drain(..).collect::<VecDeque<_>>();

    drop(lock);

    let lua = LUA_STATE.lock().unwrap().unwrap();

    while let Some(co) = coroutines.pop_front() {
        let mut nres = 0;
        let status = lua::resume(co.state, None, 0, &mut nres);

        if status == lua::LUA_YIELD {
            // coroutine yielded again, put it back into the list
            LUA_MANAGER.lock().unwrap().as_mut().unwrap().coroutines.push_back(co);
        } else if status == lua::LUA_OK {
            // coroutine finished, free the thread
            lua::L::unref(lua, lua::LUA_REGISTRYINDEX, co.thread_ref);
            lua::closethread(co.state, None);
        } else {
            // error occurred in the handler
            let errmsg = lua::tostring(co.state, -1);
            lua::L::traceback(lua, co.state, Some(&errmsg), 0);
            let traceback = lua::tostring(lua, -1);

            error!("Error occured while resuming event coroutine: {}", traceback);

            lua::pop(lua, 1); // traceback
            lua::pop(co.state, 1); // errmsg
            lua::L::unref(lua, lua::LUA_REGISTRYINDEX, co.thread_ref);
            lua::closethread(co.state, None);
        }
    }

    !LUA_MANAGER.lock().unwrap().as_ref().unwrap().coroutines.is_empty()
}

pub fn unref(ind: i64) {
    let l = LUA_STATE.lock().unwrap().unwrap();

    lua::L::unref(l, lua::LUA_REGISTRYINDEX, ind);
}

impl ToLua for String {
    fn push_to_lua(&self, l: &lua::lua_State) {
        lua::pushstring(l, &self);
    }
}

pub struct LuaLogSink {
}

impl crate::logging::Sink for LuaLogSink {
    fn write(&mut self, message: &str) {
        LOG_MESSAGES.lock().unwrap().push_back(String::from(message));
    }

    fn flush(&mut self) { }
}

impl LuaLogSink {
    pub fn new() -> Box<LuaLogSink> {
        Box::new(LuaLogSink { })
    }
}
