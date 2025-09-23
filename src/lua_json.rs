// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT

//! Lua<->JSON handling
#[allow(unused_imports)]
use crate::logging::{debug, info, warn, error};

use crate::lua;
use crate::lua::lua_State;

use crate::overlay::lua::luawarn;

pub fn pushjson(l: &lua_State, value: &serde_json::Value) {
    match value {
        serde_json::Value::Null      => lua::pushnil(l),
        serde_json::Value::Bool(b)   => lua::pushboolean(l, *b),
        serde_json::Value::Number(n) => pushnumber(l, n),
        serde_json::Value::String(s) => lua::pushstring(l, s),
        serde_json::Value::Object(o) => pushobject(l, o),
        serde_json::Value::Array(a)  => pusharray(l, a),
    }
}

fn pushnumber(l: &lua_State, n: &serde_json::Number) {
    if n.is_i64() { lua::pushinteger(l, n.as_i64().unwrap()); }
    else if n.is_f64() { lua::pushnumber(l, n.as_f64().unwrap()); }
    else { panic!("json number isn't i64 or f64."); }
}

fn pushobject(l: &lua_State, obj: &serde_json::Map<String, serde_json::Value>) {
    lua::createtable(l, 0, obj.len() as i32);

    for (key, val) in obj {
        pushjson(l, val);
        lua::setfield(l, -2, key);
    }
}

fn pusharray(l: &lua_State, arr: &Vec<serde_json::Value>) {
    lua::createtable(l, arr.len() as i32, 0);

    for i in 0..arr.len() {
        let val = &arr[i];
        pushjson(l, val);
        lua::seti(l, -2, (i+1) as i64);
    }
}

pub fn tojson(l: &lua_State, ind: i32) -> serde_json::Value {
    match lua::luatype(l, ind) {
        lua::LuaType::LUA_TNIL => serde_json::Value::Null,
        lua::LuaType::LUA_TBOOLEAN => serde_json::Value::Bool(lua::toboolean(l, ind)),
        lua::LuaType::LUA_TNUMBER => {
            let n: serde_json::Number = if lua::isinteger(l, ind) {
                serde_json::Number::from_i128(lua::tointeger(l, ind) as i128)
                    .expect("Couldn't convert Lua integer.")
            } else {
                serde_json::Number::from_f64(lua::tonumber(l, ind))
                    .expect("Couldn't convert Lua number.")
            };
            return serde_json::Value::Number(n);
        },
        lua::LuaType::LUA_TSTRING => serde_json::Value::String(String::from(lua::tostring(l, ind).unwrap())),
        lua::LuaType::LUA_TTABLE  => table_to_json(l, ind),
        lua::LuaType::LUA_TNONE |
        lua::LuaType::LUA_TLIGHTUSERDATA |
        lua::LuaType::LUA_TUSERDATA |
        lua::LuaType::LUA_TFUNCTION |
        lua::LuaType::LUA_TTHREAD => {
            luawarn!(l, "Lua type not supported for tojson.");
            return serde_json::Value::Null;
        },
    }
}

fn table_to_json(l: &lua_State, ind: i32) -> serde_json::Value {
    if table_is_valid_array(l, ind) {
        let mut arr: Vec<serde_json::Value> = Vec::new();

        let len = lua::L::len(l, ind);

        for i in 1..(len+1) {
            lua::geti(l, ind, i as i64);
            arr.push(tojson(l, lua::gettop(l)));
        }

        return serde_json::Value::Array(arr);
    } else {
        let mut obj: serde_json::Map<String, serde_json::Value> = serde_json::Map::new();

        lua::pushnil(l);

        while lua::next(l, ind) > 0 {
            lua::pushvalue(l, -2); // copy the key

            let key = lua::tostring(l, -1).unwrap_or(String::new()); // this might convert it to a string
            lua::pop(l, 1); // pop the copy

            let val = tojson(l, lua::gettop(l));

            let _ = obj.insert(key, val);
            lua::pop(l, 1); // value
        }

        return serde_json::Value::Object(obj);
    }
}

fn table_is_valid_array(l: &lua_State, ind: i32) -> bool {
    // what the next key should be. keys in a sequence that can be a valid
    // array should be integers that start at 1 and are contiguous
    let mut next_key = 1;

    lua::pushnil(l);

    while lua::next(l, ind) > 0 {
        lua::pop(l, 1); // value

        if !lua::isinteger(l, -1) {
            lua::pop(l, 1); // key
            return false;
        }

        if lua::tointeger(l, -1) != next_key {
            lua::pop(l, 1); // key
            return false;
        }

        next_key += 1;
    }

    true
}

impl crate::lua_manager::ToLua for serde_json::Value {
    fn push_to_lua(&self, l: &lua_State) {
        pushjson(l, self);
    }
}
