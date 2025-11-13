// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT

//! Overlay Lua Management

use crate::logging::{debug, info, warn, error};

use crate::lua;
use crate::utils;

use std::collections::VecDeque;

use std::sync::{Arc, Mutex, atomic};

use std::collections::{HashMap};


#[doc(hidden)]
static LUA_MANAGER: Mutex<Option<LuaManager>> = Mutex::new(None);

static LOG_MESSAGES: Mutex<VecDeque<String>> = Mutex::new(VecDeque::new());

static LUA_STATE: Mutex<Option<&lua::lua_State>> = Mutex::new(None);

static LUA_KEYBIND_STATE: Mutex<Option<KeybindState>> = Mutex::new(None);

/// The global Lua state.
struct LuaManager {
    module_openers: HashMap<String, lua::lua_CFunction>,

    events: VecDeque<LuaEvent>,
    targeted_events: VecDeque<TargetedLuaEvent>,
    event_handlers: HashMap<String, Vec<i64>>,
    keybind_handlers: HashMap<String, Vec<i64>>,

    coroutines: VecDeque<LuaCoRoutineThread>,

    unrefs: VecDeque<i64>,

    run_thread: Arc<atomic::AtomicBool>,
    thread: Option<std::thread::JoinHandle<()>>,
}

// keybind event channels
struct KeybindState {
    event_send: std::sync::mpsc::Sender<crate::input::KeyboardEvent>,
    return_recv: std::sync::mpsc::Receiver<bool>,
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
    lua::checkargstring!(l, 1);
    let name = lua::tostring(l, 1).unwrap();

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

unsafe extern "C" fn lua_print(l: &lua::lua_State) -> i32 {
    let mut msg_parts: Vec<String> = Vec::new();

    for i in 1..(lua::gettop(l) + 1) {
        match lua::luatype(l, i) {
            lua::LuaType::LUA_TNUMBER => msg_parts.push(format!("{}", lua::tonumber(l, i))),
            lua::LuaType::LUA_TBOOLEAN => msg_parts.push(format!("{}", lua::toboolean(l, i))),
            lua::LuaType::LUA_TSTRING => msg_parts.push(lua::tostring(l, i).unwrap()),
            lua::LuaType::LUA_TTABLE => msg_parts.push(String::from("<table>")),
            lua::LuaType::LUA_TFUNCTION => msg_parts.push(String::from("<function>")),
            lua::LuaType::LUA_TUSERDATA => msg_parts.push(String::from("<userdata>")),
            lua::LuaType::LUA_TTHREAD => msg_parts.push(String::from("<thread>")),
            lua::LuaType::LUA_TLIGHTUSERDATA => msg_parts.push(String::from("<lightuserdata>")),
            _ => continue,
        }
    }

    let msg = msg_parts.join(" ");

    crate::logging::log("lua", crate::logging::LoggingLevel::Info, &msg);

    return 0;
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

    // set a custom print function that outputs to the log
    lua::pushcfunction(l, Some(lua_print));
    lua::setglobal(l, "print");

    let luaman = LuaManager {
        module_openers: HashMap::new(),

        events: VecDeque::new(),
        targeted_events: VecDeque::new(),
        event_handlers: HashMap::new(),
        keybind_handlers: HashMap::new(),
        coroutines: VecDeque::new(),

        unrefs: VecDeque::new(),

        run_thread: Arc::new(atomic::AtomicBool::new(false)),
        thread: None,

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
    let state_lock = LUA_STATE.lock().unwrap();
    let l = state_lock.unwrap();

    let thread = lua::newthread(l).expect("Couldn't create Lua thread.");

    if let Err(_r) = lua::L::loadfile(thread, path) {
        let err_msg = lua::tostring(thread, -1).unwrap();
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
        let err_msg = lua::tostring(thread, -1).unwrap();
        lua::L::traceback(l, thread, Some(&err_msg), 0);
        let traceback = lua::tostring(l, -1).unwrap();
        error!("Error occured during lua run file: {}", traceback);

        lua::pop(l, 1);
        lua::pop(thread, 1);
    }

    lua::closethread(thread, None);

    // pop the thread
    lua::pop(l, 1);
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

pub fn process_keyboard_event(keyevent: &crate::input::KeyboardEvent) -> bool {
    queue_event(&keyevent.to_string(), None); // non-blocking key events

    if !keyevent.down { return false; }

    let lock = LUA_MANAGER.lock().unwrap();
    let luaman = lock.as_ref().unwrap();

    if !luaman.keybind_handlers.contains_key(&keyevent.full_name()) { return false; }

    drop(lock);

    let lock = LUA_KEYBIND_STATE.lock().unwrap();

    if lock.is_none() { return false; }

    let state = lock.as_ref().unwrap();

    while let Ok(_) = state.return_recv.try_recv() { }

    if let Err(_) = state.event_send.send(keyevent.clone()) {
        error!("Couldn't send keyboard event for Lua keybind: {}", keyevent);
        return false;
    }

    match state.return_recv.recv_timeout(std::time::Duration::from_millis(25)) {
        Ok(r) => return r,
        Err(er) => match er {
            std::sync::mpsc::RecvTimeoutError::Timeout => {
                error!("Timeout while processing keybind for {}", keyevent);
            },
            _ => {},
        },
    }

    false
}

fn process_keybinds(keyevent: &crate::input::KeyboardEvent) -> bool {
    if !keyevent.down { return false; }

    let keybinds = LUA_MANAGER.lock().unwrap().as_ref().unwrap().keybind_handlers.clone();

    let keyname = keyevent.full_name();

    if !keybinds.contains_key(&keyname) { return false; }

    let state_lock = LUA_STATE.lock().unwrap();
    let l = state_lock.unwrap();

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
                let errmsg = lua::tostring(l, -1).unwrap();
                error!("Error during keybind callback for {}: {}", keyevent, errmsg);
                lua::pop(l, 1);
            }
        }
    }

    false
}

// Some code paths, most notably when event data that is queued from Lua is dropped,
// do not have access to the Lua state in order to unreference data that has been
// referenced in the global registry.
//
// When that happens, it's stored in the queue and then unref'd later when this
// function is called.
pub fn cleanup_refs() {
    let unrefs = LUA_MANAGER.lock().unwrap().as_mut().unwrap().unrefs.drain(..).collect::<VecDeque<_>>();

    let state_lock = LUA_STATE.lock().unwrap();
    let lua = state_lock.unwrap();

    for r in unrefs {
        lua::L::unref(lua, lua::LUA_REGISTRYINDEX, r);
    }
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
    let state_lock = LUA_STATE.lock().unwrap();
    let lua = state_lock.unwrap();
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

                // this pops the thread from the stack and saves it
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
                let errmsg = lua::tostring(cothread, -1).unwrap();
                lua::L::traceback(lua, cothread, Some(&errmsg), 0);
                let traceback = lua::tostring(lua, -1).unwrap();

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
            let errmsg = lua::tostring(cothread, -1).unwrap();
            lua::L::traceback(lua, cothread, Some(&errmsg), 0);
            let traceback = lua::tostring(lua, -1).unwrap();

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

    let state_lock = LUA_STATE.lock().unwrap();
    let lua = state_lock.unwrap();

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
            let errmsg = lua::tostring(co.state, -1).unwrap();
            lua::L::traceback(lua, co.state, Some(&errmsg), 0);
            let traceback = lua::tostring(lua, -1).unwrap();

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
    let mut lock = LUA_MANAGER.lock().unwrap();
    let luaman = lock.as_mut().unwrap();

    luaman.unrefs.push_back(ind);
}

impl ToLua for String {
    fn push_to_lua(&self, l: &lua::lua_State) {
        lua::pushstring(l, &self);
    }
}

pub fn start_thread() {
    debug!("Starting Lua Thread...");

    let mut lock = LUA_MANAGER.lock().unwrap();
    let luaman = lock.as_mut().unwrap();

    luaman.run_thread.store(true, atomic::Ordering::Relaxed);

    let (event_send, event_recv) = std::sync::mpsc::channel::<crate::input::KeyboardEvent>();
    let (ret_send, ret_recv) = std::sync::mpsc::channel::<bool>();

    let state = KeybindState {
        event_send: event_send,
        return_recv: ret_recv,
    };

    *LUA_KEYBIND_STATE.lock().unwrap() = Some(state);

    let run_thread = luaman.run_thread.clone();
    let lua = std::thread::Builder::new().name("EG-Overlay Lua Thread".to_string()).spawn(move || {
        lua_thread(run_thread, event_recv, ret_send);
    }).expect("Couldn't spawn Lua thread.");

    luaman.thread = Some(lua);
}

pub fn stop_thread() {
    let mut lock = LUA_MANAGER.lock().unwrap();
    let luaman = lock.as_mut().unwrap();

    luaman.run_thread.store(false, atomic::Ordering::Relaxed);

    let thread = luaman.thread.take().unwrap();

    drop(lock);
    thread.join().expect("Lua thread panicked.");

    *LUA_KEYBIND_STATE.lock().unwrap() = None;
}

fn lua_thread(
    run_thread: Arc<atomic::AtomicBool>,
    keyboard_event_recv: std::sync::mpsc::Receiver<crate::input::KeyboardEvent>,
    keybind_return_send: std::sync::mpsc::Sender<bool>
) {
    debug!("Begin Lua thread.");

    let overlay = crate::overlay::overlay();

    utils::init_com_for_thread();

    info!("Running lua/autoload.lua...");
    run_file("lua/autoload.lua");

    queue_event("startup", None);
    run_event_queue();

    let update_target = overlay.settings().get_f64("overlay.luaUpdateTarget").unwrap();

    debug!("Lua update target time: {}ms or ~{:.0} times per second.", update_target, 1000.0 / update_target);

    while run_thread.load(atomic::Ordering::Relaxed) {
        let lua_begin = overlay.uptime().as_secs_f64();

        if let Ok(keyevent) = keyboard_event_recv.try_recv() {
            keybind_return_send.send(process_keybinds(&keyevent)).unwrap();
        }

        cleanup_refs();
        resume_coroutines();
        queue_event("update", None);
        run_event_queue();

        let mut lua_end = overlay.uptime().as_secs_f64();

        let mut lua_time = (lua_end - lua_begin) * 1000.0;
        let mut sleep_time = update_target - lua_time;

        // We only want to send 'update' events at a certain interval, by default
        // around 30 times per second.
        loop {
            // we are out of time, star the loop again to send another 'update'
            if sleep_time <= 0.0 { break; }

            // if not, resume any coroutines that haven't finished
            let more_coroutines = resume_coroutines();

            // recalculate how much time is left before the next update
            lua_end = overlay.uptime().as_secs_f64();
            lua_time = (lua_end - lua_begin) * 1000.0;
            sleep_time = update_target - lua_time;

            // if there are no pending coroutines then just sleep until the next
            // 'update' needs to go out, if there is time
            if !more_coroutines { break; }
        }

        if sleep_time > 0.0 {
            // sleep the rest of the time, except if a keybound keyboard event comes in
            if let Ok(keyevent) = keyboard_event_recv.recv_timeout(std::time::Duration::from_secs_f64(sleep_time / 1000.0)) {
                keybind_return_send.send(process_keybinds(&keyevent)).unwrap();
            }
        }
    }

    utils::uninit_com_for_thread();
    debug!("End Lua thread.");
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
