// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT

/*** RST
Settings
========

.. lua:class:: settingsstore

    Settings store allow modules to store data that is backed by JSON files.

    Create a settings store with :lua:func:`eg-overlay.settings`.
*/
#[allow(unused_imports)]
use crate::logging::{debug, info, warn, error};

use std::sync::Arc;
use std::mem::ManuallyDrop;

use crate::lua;
use crate::lua::lua_State;
use crate::lua::luaL_Reg;
use crate::lua::luaL_Reg_list;

use crate::settings::SettingsStore;

use crate::lua_json;

const SETTINGS_METATABLE_NAME: &str = "SettingsStore";

const SETTINGS_FUNCS: &[luaL_Reg] = luaL_Reg_list!{
    c"__gc"      , __gc,
    c"setdefault", set_default,
    c"get"       , get,
    c"set"       , set,
    c"remove"    , remove,
};


pub fn pushsettings(l: &lua_State, settings: Arc<SettingsStore>) {
    let settings_ptr = Arc::into_raw(settings.clone()); // strong count ++

    let lua_settings_ptr: *mut *const SettingsStore = unsafe {
        std::mem::transmute(lua::newuserdatauv(l, std::mem::size_of::<*const SettingsStore>(), 0))
    };

    unsafe { *lua_settings_ptr = settings_ptr; }

    if lua::L::newmetatable(l, SETTINGS_METATABLE_NAME) {
        lua::pushvalue(l, -1);
        lua::setfield(l, -2, "__index");
        
        lua::L::setfuncs(l, SETTINGS_FUNCS, 0);
    }
    lua::setmetatable(l, -2);
}

pub unsafe fn checksettings_ptr(l: &lua_State, ind: i32) -> *mut *const SettingsStore {
    unsafe {
        std::mem::transmute(lua::L::checkudata(l, ind, SETTINGS_METATABLE_NAME))
    }
}

pub unsafe fn checksettings(l: &lua_State, ind: i32) -> ManuallyDrop<Arc<SettingsStore>> {
    ManuallyDrop::new(unsafe { Arc::from_raw(*checksettings_ptr(l, ind)) })
}

unsafe extern "C" fn __gc(l: &lua_State) -> i32 {
    let settings_ptr = unsafe { checksettings_ptr(l, 1) };

    let s: Arc<SettingsStore> = unsafe { Arc::from_raw(*settings_ptr) };

    drop(s);

    return 0;
}

/*** RST
    .. lua:method:: setdefault(key, value)

        Set a default value to be used if ``key`` does not have a value set.

        ``value`` must be a `number`, `boolean`, `nil`, or `string`.

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn set_default(l: &lua_State) -> i32 {
    let s = unsafe { checksettings(l, 1) };
    let key = unsafe { lua::L::checkstring(l, 2) };

    let val = lua_json::tojson(l, 3);

    s.set_default_value(&key, val);

    return 0;
}

/*** RST
    .. lua:method:: get(key)

        :returns: The value for the given key, or nil if no value exists.

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn get(l: &lua_State) -> i32 {
    let s = unsafe { checksettings(l, 1) };
    let key = unsafe { lua::L::checkstring(l, 2) };

    if let Some(val) = s.get(&key) {
        lua_json::pushjson(l, &val);
    } else {
        lua::pushnil(l);
    }

    return 1;
}

/*** RST
    .. lua:method:: set(key, value)

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn set(l: &lua_State) -> i32 {
    let s = unsafe { checksettings(l, 1) };
    let key = unsafe { lua::L::checkstring(l, 2) };

    let val = lua_json::tojson(l, 3);

    s.set(&key, val);

    return 0;
}

/*** RST
    .. lua:method:: remove(key)

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn remove(l: &lua_State) -> i32 {
    lua::checkargstring!(l, 2);

    let s = unsafe { checksettings(l, 1) };
    let key = lua::tostring(l, 2);

    s.remove(&key);

    return 0;
}
