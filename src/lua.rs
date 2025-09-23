// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT

//! Lua API

#![allow(non_snake_case)]
#![allow(non_camel_case_types)]
#![allow(dead_code)]

use std::ffi::{CStr, CString};

use std::ffi::{c_int, c_longlong, c_double, c_char, c_void};

/* Core lua.h stuff first */

pub type lua_Integer = c_longlong;
pub type lua_Number  = c_double;

pub type lua_KContext = *const usize;

const LUAI_MAXSTACK: c_int = 1000000;

pub const LUA_MULTRET: c_int = -1;

pub const LUA_REGISTRYINDEX: c_int = -LUAI_MAXSTACK - 1000;

pub const LUA_OK       : c_int = 0;
pub const LUA_YIELD    : c_int = 1;
pub const LUA_ERRRUN   : c_int = 2;
pub const LUA_ERRSYNTAX: c_int = 3;
pub const LUA_ERRMEM   : c_int = 4;
pub const LUA_ERRERR   : c_int = 5;

pub const LUA_OPEQ: c_int = 0;
pub const LUA_OPLT: c_int = 1;
pub const LUA_OPLE: c_int = 2;

/// Opaque pointer to a Lua state/thread.
#[repr(C)]
pub struct lua_State {
    _foo: [u8;0],
}

/// The type of a Lua value on the stack.
///
/// Returned by [luatype] and other functions.
#[repr(C)]
#[derive(PartialEq,Eq)]
pub enum LuaType {
    LUA_TNONE          = -1,
    LUA_TNIL           =  0,
    LUA_TBOOLEAN       =  1,
    LUA_TLIGHTUSERDATA =  2,
    LUA_TNUMBER        =  3,
    LUA_TSTRING        =  4,
    LUA_TTABLE         =  5,
    LUA_TFUNCTION      =  6,
    LUA_TUSERDATA      =  7,
    LUA_TTHREAD        =  8,
}

impl LuaType {
    pub fn as_str(&self) -> &str {
        match self {
            LuaType::LUA_TNONE          => "none",
            LuaType::LUA_TNIL           => "nil",
            LuaType::LUA_TBOOLEAN       => "boolean",
            LuaType::LUA_TLIGHTUSERDATA => "lightuserdata",
            LuaType::LUA_TNUMBER        => "number",
            LuaType::LUA_TSTRING        => "string",
            LuaType::LUA_TTABLE         => "table",
            LuaType::LUA_TFUNCTION      => "function",
            LuaType::LUA_TUSERDATA      => "userdata",
            LuaType::LUA_TTHREAD        => "thread",
        }
    }
}

/// A Rust function that can be called from Lua.
pub type lua_CFunction = Option<unsafe extern "C" fn(&lua_State) -> c_int>;

pub type lua_KFunction = Option<unsafe extern "C" fn(&lua_State, c_int, lua_KContext) -> c_int>;

#[repr(C)]
pub struct lua_Debug {
    pub event: c_int,
    pub name    : *const c_char,
    pub namewhat: *const c_char,
    pub what    : *const c_char,
    pub source  : *const c_char,
    pub srclen: usize,
    pub currentline: c_int,
    pub linedefined: c_int,
    pub lastlinedefined: c_int,
    pub nups: u8,
    pub nparams: u8,
    pub isvararg: c_char,
    pub istailcall: c_char,
    pub ftransfer: u16,
    pub ntransfer: u16,
    pub short_src: [c_char; 60],

    i_ci: *const c_void,
}

impl Default for lua_Debug {
    fn default() -> lua_Debug {
        lua_Debug {
            event: 0,
            name: 0 as *const c_char,
            namewhat: 0 as *const c_char,
            what: 0 as *const c_char,
            source: 0 as *const c_char,
            srclen: 0,
            currentline: 0,
            linedefined: 0,
            lastlinedefined: 0,
            nups: 0,
            nparams: 0,
            isvararg: 0,
            istailcall: 0,
            ftransfer: 0,
            ntransfer: 0,
            short_src: [0; 60],

            i_ci: 0 as *const c_void,
        }
    }
}

/// Creates a new Lua thread.
///
/// The newly created thread is pushed onto the stack of `state` and the new
/// thread's state is returned.
pub fn newthread(state: &lua_State) -> Result<&lua_State,()> {
    let l = unsafe { api::lua_newthread(state) };

    if l.is_some() {
        return Ok(l.unwrap());
    }

    Err(())
}

pub fn createtable(state: &lua_State, narr: i32, nrec: i32) {
    unsafe { api::lua_createtable(state, narr, nrec) };
}

/// Creates a new empty table and pushes it onto the stack.
pub fn newtable(state: &lua_State) {
    unsafe { api::lua_createtable(state, 0, 0) };
}

/// Closes a Lua thread.
///
/// Close all active to-be-closed variables in the main thread, release all
/// objects in the given Lua state (calling the corresponding garbage-collection
/// metamethods, if any), and frees all dynamic memory used by this state.
pub fn close(state: &lua_State) {
    unsafe { api::lua_close(state) };
}

/// Pops `n` elements from the stack.
pub fn pop(state: &lua_State, n: i32) {
    unsafe {
        api::lua_settop(state, -(n)-1);
    }
}

/// Pushes onto the stack the value of the global `name`. Returns the type of
/// that value.
pub fn getglobal(state: &lua_State, name: &str) -> LuaType {
    let cstr = CString::new(name).unwrap();
    unsafe {
        return api::lua_getglobal(state, cstr.as_ptr());
    }
}

/// Pushes onto the stack the value `t[k]`.
///
/// Where `t` is the value at the given `index`. As in Lua, this function may
/// trigger a metamethod for the "index" event.
pub fn getfield(state: &lua_State, index: i32, k: &str) -> LuaType {
    let ck = CString::new(k).unwrap();
    unsafe {
        return api::lua_getfield(state, index, ck.as_ptr());
    }
}

pub fn geti(state: &lua_State, index: i32, n: i64) -> LuaType {
    unsafe { api::lua_geti(state, index, n) }
}

/// Does the equivalent to `t[n] = v`.
///
/// Where `t` is the value at the given `index` and `v` is the value at the top
/// of the stack.
///
/// This function pops the value from the stack. As in Lua, this function may
/// trigger a metamethod for the "newindex" event.
pub fn seti(state: &lua_State, index: i32, n: i64) {
    unsafe {
        api::lua_seti(state, index, n);
    }
}

/// Does the equivalent to `t[k] = v`.
///
/// Where `t` is the value at the given index and `v` is the value on the top of
/// the stack.
///
/// This function pops the value from the stack. As in Lua, this function may
/// trigger a metamethod for the "newindex" event.
pub fn setfield(state: &lua_State, index: i32, k: &str) {
    let ck = CString::new(k).unwrap();
    unsafe {
        api::lua_setfield(state, index, ck.as_ptr());
    }
}

/// Pops a value from the stack and sets it as the new value of global `name`.
pub fn setglobal(state: &lua_State, name: &str) {
    let cname = CString::new(name).unwrap();
    unsafe { api::lua_setglobal(state, cname.as_ptr()) };
}

/// Pushes a boolean value with value `b` onto the stack.
pub fn pushboolean(state: &lua_State, b: bool) {
    if b { unsafe {
        api::lua_pushboolean(state, 1);
    } } else { unsafe {
        api::lua_pushboolean(state, 0);
    } }
}

/// Pushes a Rust function onto the stack.
///
/// This function is equivalent to [pushcclosure] with no upvalues.
pub fn pushcfunction(state: &lua_State, func: lua_CFunction) {
    pushcclosure(state, func, 0);
}

/// Pushes a signed integer value onto the stack.
pub fn pushinteger(state: &lua_State, n: i64) {
    unsafe {
        api::lua_pushinteger(state, n);
    }
}

/// Pushes a new Rust closure onto the stack.
///
/// This function receives a pointer to a function and pushes onto the stack a
/// Lua value of the type `function` that, when called, invokes the corresponding
/// Rust function. The parameter `n` tells how many upvalues this function will
/// have.
pub fn pushcclosure(state: &lua_State, func: lua_CFunction, n: i32) {
    unsafe {
        api::lua_pushcclosure(state, func, n);
    }
}

pub unsafe fn pushlightuserdata(state: &lua_State, p: *const c_void) {
    unsafe {
        api::lua_pushlightuserdata(state, p);
    }
}

/// Pushes the string `s` onto the stack.
///
/// Lua will make an internal copy of the string.
pub fn pushstring(state: &lua_State, value: &str) {
    let cstr = std::ffi::CString::new(value).unwrap();
    unsafe {
        api::lua_pushstring(state, cstr.as_ptr());
    }
}

/// Pushes a nil value onto the stack.
pub fn pushnil(state: &lua_State) {
    unsafe { api::lua_pushnil(state); }
}

/// Pushes a float with value `n` onto the stack.
pub fn pushnumber(state: &lua_State, n: f64) {
    unsafe { api::lua_pushnumber(state, n) };
}

/// Pushes a copy of the element at the given index onto the stack.
pub fn pushvalue(state: &lua_State, index: i32) {
    unsafe { api::lua_pushvalue(state, index) };
}

/// Converts the Lua value at the given index to a string and returns it.
pub fn tostring(state: &lua_State, ind: i32) -> Option<String> {
    let cstr = unsafe { api::lua_tolstring(state, ind, 0 as *mut usize) };

    if cstr.is_null() { return None; }

    Some(unsafe { CStr::from_ptr(cstr).to_string_lossy().into_owned() })
}

/// Converts the Lua value at the given index to a string and returns it as an
/// array of bytes.
///
/// The returned value may contain nulls.
///
/// This corresponds to the C function `lua_tolstring`.
pub fn tobytes<T>(state: &lua_State, ind: i32) -> &'static [T] {
    let mut l = 0;
    let ptr = unsafe { api::lua_tolstring(state, ind, &mut l) };

    unsafe { std::slice::from_raw_parts(ptr as *const T, l) }
}

pub fn pushbytes(state: &lua_State, b: &[i8]) {
    unsafe { api::lua_pushlstring(state, b.as_ptr(), b.len()); }
}

/// Calls a function (or a callable object) in protected mode.
///
/// Both ``nargs`` and ``nresults`` have the same meaning as in lua_call. If
/// there are no errors during the call, lua_pcall behaves exactly like lua_call.
/// However, if there is any error, lua_pcall catches it, pushes a single value
/// on the stack (the error object), and returns an error code. Like lua_call,
/// lua_pcall always removes the function and its arguments from the stack.
///
/// If ``msgh`` is 0, then the error object returned on the stack is exactly the
/// original error object. Otherwise, ``msgh`` is the stack index of a message
/// handler. (This index cannot be a pseudo-index.) In case of runtime errors,
/// this handler will be called with the error object and its return value will
/// be the object returned on the stack by pcall.
///
/// Typically, the message handler is used to add more debug information to the
/// error object, such as a stack traceback. Such information cannot be gathered
/// after the return of pcall, since by then the stack has unwound.
///
/// The pcall function returns one of the following status codes in either an
/// ``Ok()`` or an ``Err()``:
///
/// - LUA_OK
/// - LUA_ERRRUN
/// - LUA_ERRMEM
/// - LUA_ERRERR
pub fn pcall(state: &lua_State, nargs: i32, nresults: i32, errfunc: i32) -> Result<i32, i32> {
    unsafe {
        let r =api::lua_pcallk(state, nargs, nresults, errfunc, 0 as *mut usize, None);
        if r==LUA_OK {
            return Ok(r);
        }
        Err(r)
    }
}

/// Starts and resumes a coroutine in the given thread `state`.
///
/// To start a coroutine, you push the main function plus any arguments onto
/// the empty stack of the thread. then you call [resume], with `nargs` being
/// the number of arguments. This call returns when the coroutine suspends or
/// finishes its execution. When it returns, *nresults is updated and the top of
/// the stack contains the *nresults values passed to lua_yield or returned by
/// the body function. lua_resume returns [LUA_YIELD] if the coroutine yields,
/// [LUA_OK] if the coroutine finishes its execution without errors, or an error
/// code in case of errors. In case of errors, the error object is on the top of
/// the stack.
///
/// To resume a coroutine, you remove the *nresults yielded values from its
/// stack, push the values to be passed as results from yield, and then call
/// [resume].
///
/// The parameter `from` represents the coroutine that is resuming `state`. If
/// there is no such coroutine, this parameter can be [None]. 
pub fn resume(state: &lua_State, from: Option<&lua_State>, nargs: i32, nresults: &mut i32) -> i32 {
    return unsafe {
        api::lua_resume(state, from, nargs, nresults)
    };
}

///  Resets a thread, cleaning its call stack and closing all pending
/// to-be-closed variables.
///
/// Returns a status code: [LUA_OK] for no errors in the thread (either the
/// original error that stopped the thread or errors in closing methods), or
/// an error status otherwise. In case of error, leaves the error object on the
/// top of the stack.
///
/// The parameter `from` represents the coroutine that is resetting `state`. If
/// there is no such coroutine, this parameter can be [None]. 
pub fn closethread(state: &lua_State, from: Option<&lua_State>) -> i32 {
    unsafe { api::lua_closethread(state, from) }
}

/// Calls a function.
///
/// Like regular Lua calls, [call] respects the `__call` metamethod. So, here
/// the word "function" means any callable value.
///
/// To do a call you must use the following protocol: first, the function to be
/// called is pushed onto the stack; then, the arguments to the call are pushed
/// in direct order; that is, the first argument is pushed first. Finally you
/// call [call]; `nargs` is the number of arguments that you pushed onto the
/// stack. When the function returns, all arguments and the function value are
/// popped and the call results are pushed onto the stack. The number of results
/// is adjusted to `nresults`, unless `nresults` is [LUA_MULTRET]. In this case,
/// all results from the function are pushed; Lua takes care that the returned
/// values fit into the stack space, but it does not ensure any extra space in
/// the stack. The function results are pushed onto the stack in direct order
/// (the first result is pushed first), so that after the call the last result
/// is on the top of the stack.
///
/// Any error while calling and running the function is propagated upwards (with
/// a longjmp).
///
/// The following example shows how the host program can do the equivalent to
/// this Lua code:
///
/// ```lua
/// a = f("how", t.x, 14)
/// ```
///
/// Here it is in rust:
/// 
/// ```rust
/// use lua;
///
/// lua::getglobal(L, "f");                  /* function to be called */
/// lua::pushliteral(L, "how");                       /* 1st argument */
/// lua::getglobal(L, "t");                    /* table to be indexed */
/// lua::getfield(L, -1, "x");        /* push result of t.x (2nd arg) */
/// lua::remove(L, -2);                  /* remove 't' from the stack */
/// lua::pushinteger(L, 14);                          /* 3rd argument */
/// lua::call(L, 3, 1);     /* call 'f' with 3 arguments and 1 result */
/// lua::setglobal(L, "a");                         /* set global 'a' */
/// ```
///
/// Note that the code above is balanced: at its end, the stack is back to its
/// original configuration. This is considered good programming practice. 
pub fn call(state: &lua_State, nargs: i32, nresults: i32) {
    unsafe { api::lua_callk(state, nargs, nresults, 0 as *mut usize, None) };
}

/// Raises a Lua error, using the value on the top of the stack as the error object.
///
/// This function does a long jump, and therefore never returns.
pub unsafe fn error(state: &lua_State) -> i32 {
    unsafe { api::lua_error(state) }
}

/// Creates a new full userdata value.
///
/// This function creates and pushes on the stack a new full userdata, with
/// `nuvalue` associated Lua values, called user values, plus an associated
/// block of raw memory with size `bytes`. (The user values can be set and read
/// with the functions setiuservalue and getiuservalue.)
///
/// The function returns the address of the block of memory. Lua ensures that
/// this address is valid as long as the corresponding userdata is alive.
/// Moreover, if the userdata is marked for finalization, its address is valid
/// at least until the call to its finalizer. 
pub unsafe fn newuserdatauv(state: &lua_State, size: usize, nuvalue: i32) -> *mut std::ffi::c_void {
    unsafe { api::lua_newuserdatauv(state, size, nuvalue) }
}

/// Set a value's metatable.
///
/// Pops a table or nil from the stack and sets that value as the new metatable
/// for the value at the given index. (nil means no metatable.)
pub fn setmetatable(state: &lua_State, index: i32) {
    unsafe { api::lua_setmetatable(state, index) };
}

/// Returns a value's metatable.
///
/// If the value at the given `index` has a metatable, the function pushes that
/// metatable onto the stack and returns [true]. Otherwise, the function returns
/// [false] and pushes nothing on the stack. 
pub fn getmetatable(state: &lua_State, index: i32) -> bool {
    unsafe { api::lua_getmetatable(state, index) == 1 }
}

/// Returns the type of the value in the given valid index.
///
/// Returns [LuaType::LUA_TNONE] for a non-valid but acceptable index.
///
/// The types returned by lua_type are coded by the [LuaType]:
/// - [LuaType::LUA_TNIL]
/// - [LuaType::LUA_TNUMBER]
/// - [LuaType::LUA_TBOOLEAN]
/// - [LuaType::LUA_TSTRING]
/// - [LuaType::LUA_TTABLE]
/// - [LuaType::LUA_TFUNCTION]
/// - [LuaType::LUA_TUSERDATA]
/// - [LuaType::LUA_TTHREAD]
/// - [LuaType::LUA_TLIGHTUSERDATA]
pub fn luatype(state: &lua_State, index: i32) -> LuaType {
    unsafe { api::lua_type(state, index) }
}

/// Returns the index of the top element in the stack.
///
/// Because indices start at 1, this result is equal to the number of elements
/// in the stack; in particular, 0 means an empty stack. 
pub fn gettop(state: &lua_State) -> i32 {
    unsafe { api::lua_gettop(state) }
}

pub fn remove(state: &lua_State, index: i32) {
    unsafe { api::lua_rotate(state, index, -1); }
    pop(state, 1);
}

/// Starts or continues iteration of a Lua table.
///
/// Pops a key from the stack, and pushes a key–value pair from the table at
/// the given index, the "next" pair after the given key. If there are no more
/// elements in the table, then lua_next returns 0 and pushes nothing.
///
/// While traversing a table, avoid calling [tostring] directly on a key,
/// unless you know that the key is actually a string. Recall that [tostring]
/// may change the value at the given index; this confuses the next call to [next].
///
/// This function may raise an error if the given key is neither nil nor present
/// in the table. See function next for the caveats of modifying the table during
/// its traversal.
pub fn next(state: &lua_State, index: i32) -> i32 {
    unsafe { api::lua_next(state, index) }
}

/// Converts the Lua value at the given index to a signed integer.
pub fn tointeger(state: &lua_State, index: i32) -> i64 {
    unsafe { api::lua_tointegerx(state, index, std::ptr::null_mut()) }
}

/// Returns 1 if the value at the given index is an integer (that is, the value
/// is a number and is represented as an integer), and 0 otherwise.
pub fn isinteger(state: &lua_State, index: i32) -> bool {
    unsafe { api::lua_isinteger(state, index) == 1 }
}

pub fn tonumber(state: &lua_State, index: i32) -> f64 {
    unsafe { api::lua_tonumberx(state, index, std::ptr::null_mut()) }
}

/// Returns a full userdata value.
///
/// If the value at the given index is a full userdata, returns its memory-block
/// address. If the value is a light userdata, returns its value (a pointer).
/// Otherwise, returns NULL.
pub unsafe fn touserdata(state: &lua_State, index: i32) -> *mut std::ffi::c_void {
    unsafe { api::lua_touserdata(state, index) }
}

/// Converts the Lua value at the given index to a Rust [bool].
///
/// Like all tests in Lua, [toboolean] returns [true] for any Lua value different
/// from `false` and `nil`; otherwise it returns [false]. (If you want to accept
/// only actual boolean values, use isboolean to test the value's type.) 
pub fn toboolean(state: &lua_State, index: i32) -> bool {
    unsafe { api::lua_toboolean(state, index) == 1 }
}

/// Pushes onto the stack the value `t[n]`, where t is the table at the given
/// index.
///
/// The access is raw, that is, it does not use the __index metavalue. Returns
/// the type of the pushed value. 
pub fn rawgeti(state: &lua_State, index: i32, n: i64) -> LuaType {
    unsafe { api::lua_rawgeti(state, index, n) }
}


pub fn getstack(state: &lua_State, level: i32, ar: &mut lua_Debug) -> Result<(),()> {
    if unsafe { api::lua_getstack(state, level, ar) } == 1 { Ok(()) }
    else { Err(()) }
}

pub fn getinfo(state: &lua_State, what: &str, ar: &mut lua_Debug) -> Result<(),()> {
    let cstr = std::ffi::CString::new(what).unwrap();
    if unsafe { api::lua_getinfo(state, cstr.as_ptr(), ar) } == 1 { Ok(()) }
    else { Err(()) }
}

pub fn compare(state: &lua_State, index1: i32, index2: i32, op: i32) -> bool {
    unsafe { api::lua_compare(state, index1, index2, op) == 1 }
}

/// FFI Lua API
mod api {
    use std::ffi::{c_int, c_char, c_void};
    use crate::lua::{lua_State, lua_Number, lua_Integer, lua_KContext, LuaType, lua_CFunction, lua_Debug};

    #[link(name="lua-5.4.7")]
    unsafe extern "C" {
        pub fn lua_compare(state: &lua_State, index1: c_int, index2: c_int, op: c_int) -> c_int;

        pub fn lua_getstack(state: &lua_State, level: c_int, ar: *mut lua_Debug) -> c_int;
        pub fn lua_getinfo(state: &lua_State, what: *const c_char, ar: *mut lua_Debug) -> c_int;

        pub fn lua_newuserdatauv(state: &lua_State, size: usize, nuvalue: c_int) -> *mut std::ffi::c_void;

        pub fn lua_close(state: &lua_State);
        pub fn lua_closethread(state: &lua_State, from: Option<&lua_State>) -> c_int;

        pub fn lua_settop(state: &lua_State, index: c_int);

        pub fn lua_error(state: &lua_State) -> c_int;

        pub fn lua_type(state: &lua_State, index: c_int) -> LuaType;

        pub fn lua_gettop(state: &lua_State) -> c_int;
        pub fn lua_rotate(state: &lua_State, index: c_int, n: c_int);

        pub fn lua_next(state: &lua_State, index: i32) -> c_int;

        pub fn lua_getglobal(state: &lua_State, name: *const c_char) -> LuaType;
        pub fn lua_getfield(state: &lua_State, index: c_int, k: *const c_char) -> LuaType;
        pub fn lua_geti(state: &lua_State, index: c_int, n: lua_Integer) -> LuaType;

        pub fn lua_pushboolean(state: &lua_State, b: c_int);
        pub fn lua_pushcclosure(state: &lua_State, func: lua_CFunction, n: c_int);
        pub fn lua_pushinteger(state: &lua_State, n: lua_Integer);
        pub fn lua_pushstring(state: &lua_State, value: *const c_char);
        pub fn lua_pushlstring(state: &lua_State, s: *const c_char, l: usize);
        pub fn lua_pushnil(state: &lua_State);
        pub fn lua_pushnumber(state: &lua_State, n: lua_Number);
        pub fn lua_pushvalue(state: &lua_State, index: c_int);
        pub fn lua_pushlightuserdata(state: &lua_State, p: *const c_void);

        pub fn lua_seti(state: &lua_State, index: c_int, n: lua_Integer);
        pub fn lua_setfield(state: &lua_State, index: c_int, k: *const c_char);
        pub fn lua_setglobal(state: &lua_State, name: *const c_char);

        pub fn lua_getmetatable(state: &lua_State, index: c_int) -> c_int;
        pub fn lua_setmetatable(state: &lua_State, index: c_int) -> c_int;

        pub fn lua_rawgeti(state: &lua_State, index: c_int, n: lua_Integer) -> LuaType;

        pub fn lua_createtable(state: &lua_State, narr: c_int, nrec: c_int);

        pub fn lua_tolstring(state: &lua_State, index: c_int, l: *mut usize) -> *const c_char;
        pub fn lua_tointegerx(state: &lua_State, index: c_int, isnum: *mut c_int) -> lua_Integer;
        pub fn lua_tonumberx(state: &lua_State, index: c_int, isnum: *mut c_int) -> lua_Number;
        pub fn lua_toboolean(state: &lua_State, index: c_int) -> c_int;

        pub fn lua_isinteger(state: &lua_State, index: c_int) -> c_int;

        pub fn lua_touserdata(state: &lua_State, index: c_int) -> *mut c_void;

        pub fn lua_newthread(state: &lua_State) -> Option<&lua_State>;

        pub fn lua_resume(state: &lua_State, from: Option<&lua_State>, nargs: c_int, nresults: *mut c_int) -> c_int;

        pub fn lua_callk(
            state: &lua_State,
            nargs: c_int,
            nresults: c_int,
            ctx: lua_KContext,
            k: crate::lua::lua_KFunction
        );

        pub fn lua_pcallk(
            state: &lua_State,
            nargs: c_int,
            nresults: c_int,
            errfunc: c_int,
            ctx: lua_KContext,
            k: crate::lua::lua_KFunction
        ) -> c_int;
    }

}

pub fn dbg_name(l: &lua_State) -> String {
    let mut dbg = lua_Debug::default();

    getstack(l, 0, &mut dbg).unwrap();
    getinfo(l, "n", &mut dbg).unwrap();

    let name = unsafe { std::ffi::CStr::from_ptr(dbg.name).to_str().unwrap() };

    String::from(name)
}

macro_rules! checkarg {
    ($lua:ident, $ind:literal) => {{
        if crate::lua::gettop($lua) < $ind {
            lua::pushstring($lua, format!("missing argument #{} to function {}", $ind, $crate::lua::dbg_name($lua)).as_str());
            unsafe { lua::error($lua) };
        }
    }}
}
pub(crate) use checkarg;

macro_rules! checkargstring {
    ($lua:ident, $ind:literal) => {{
        $crate::lua::checkarg!($lua, $ind);

        if $crate::lua::luatype($lua, $ind) != $crate::lua::LuaType::LUA_TSTRING {
            lua::pushstring($lua, format!("expecting string for argument #{} to function {}, found {}",
                    $ind,
                    $crate::lua::dbg_name($lua),
                    $crate::lua::luatype($lua, $ind).as_str()
                ).as_str()
            );
            unsafe { lua::error($lua) };
        }
    }}
}
pub(crate) use checkargstring;

macro_rules! checkargnumber {
    ($lua:ident, $ind:literal) => {{
        $crate::lua::checkarg!($lua, $ind);

        if $crate::lua::luatype($lua, $ind) != $crate::lua::LuaType::LUA_TNUMBER {
            lua::pushstring($lua, format!("expecting number for argument #{} to function {}, found {}",
                    $ind,
                    $crate::lua::dbg_name($lua),
                    $crate::lua::luatype($lua, $ind).as_str()
                ).as_str()
            );
            unsafe { lua::error($lua) };
        }
    }}
}
pub(crate) use checkargnumber;

macro_rules! checkarginteger {
    ($lua:ident, $ind:literal) => {{
        $crate::lua::checkarg!($lua, $ind);
        $crate::lua::checkargnumber!($lua, $ind);

        if !$crate::lua::isinteger($lua, $ind) {
            lua::pushstring($lua, format!("argument #{} to functon {} is a number, but not an integer",
                    $ind,
                    $crate::lua::dbg_name($lua)
                ).as_str()
            );
            unsafe { lua::error($lua) };
        }
    }}
}
pub(crate) use checkarginteger;

/* lauxlib.h */

/// A definition of a function to be registered by [L::setfuncs].
#[repr(C)]
pub struct luaL_Reg {
    pub name: *const c_char,
    pub func: lua_CFunction,
}

/// Creates a list of [luaL_Reg]s to be passed to [L::setfuncs].
macro_rules! luaL_Reg_list {
    {$( $name:literal, $func:expr$(,)?),+} => {&[
        $( $crate::lua::luaL_Reg { name: $name.as_ptr(), func: Some($func) } ),+,
        $crate::lua::luaL_Reg { name: std::ptr::null(), func: None }
    ]}
}
pub(crate) use luaL_Reg_list as luaL_Reg_list;

/// Lua auxiliary library API
pub mod L {
    use crate::lua::lua_State;
    use std::ffi::{CStr, CString};

    pub unsafe fn checktype(state: &lua_State, ind: i32, type_: crate::lua::LuaType) {
        unsafe { api::luaL_checktype(state, ind, type_ as i32); }
    }

    /// Creates a new Lua state.
    ///
    /// It calls lua_newstate with an allocator based on the ISO C allocation
    /// functions and then sets a warning function and a panic function that
    /// print messages to the standard error output.
    ///
    /// Returns [Ok] with new state, or [Err] if there is a memory allocation error. 
    pub fn newstate() -> Result<&'static lua_State, ()> {
        let l = unsafe { api::luaL_newstate() };

        if l.is_some() {
            return Ok(l.unwrap());
        }

        Err(())
    }

    /// Opens all standard Lua libraries into the given state.
    pub fn openlibs(state: &lua_State) {
        unsafe { api::luaL_openlibs(state) };
    }

    /// Checks whether the function argument `arg` is a string and returns this string.
    /// 
    /// This function uses lua_tolstring to get its result, so all conversions and
    /// caveats of that function apply here. 
    pub unsafe fn checkstring(state: &lua_State, ind: i32) -> String {
        let cstr = unsafe { api::luaL_checklstring(state, ind, 0 as *mut usize) };

        unsafe { CStr::from_ptr(cstr).to_string_lossy().into_owned() }
    }

    /// Checks whether the function argument `arg` is an integer (or can be
    /// converted to an integer) and returns this integer.
    pub unsafe fn checkinteger(state: &lua_State, ind: i32) -> i64 {
        unsafe { api::luaL_checkinteger(state, ind) }
    }

    pub unsafe fn checknumber(state: &lua_State, ind: i32) -> f64 {
        unsafe { api::luaL_checknumber(state, ind) }
    }

    /// Returns the "length" of the value at the given index as a number.
    ///
    /// This is equivalent to the '#' operator in Lua. Raises an error if the
    /// result of the operation is not an integer. (This case can only happen
    /// through metamethods.) 
    pub fn len(state: &lua_State, ind: i32) -> usize {
        unsafe {
            return api::luaL_len(state, ind);
        }
    }

    /// Grows the stack size to top + `sz` elements.
    ///
    /// This raises an error if the stack cannot grow to that size. `msg` is an
    /// additional text to go into the error message. 
    pub unsafe fn checkstack(state: &lua_State, sz: i32, msg: &str) {
        let cmsg = CString::new(msg).unwrap();
        unsafe {
            api::luaL_checkstack(state, sz, cmsg.as_ptr());
        }
    }

    /// Equivalent to [loadfilex] with `mode` equal to [None]. 
    pub fn loadfile(state: &lua_State, filename: &str) -> Result<i32, i32> {
        return loadfilex(state, filename, None);
    }

    /// Loads a file as a Lua chunk.
    ///
    /// This function uses lua_load to load the chunk in the file named `filename`.
    /// The first line in the file is ignored if it starts with a #.
    /// 
    /// The string mode works as in the function lua_load.
    ///
    /// This function returns the same results as lua_load or LUA_ERRFILE for
    /// file-related errors.
    ///
    /// As lua_load, this function only loads the chunk; it does not run it. 
    pub fn loadfilex(state: &lua_State, filename: &str, mode: Option<&str>) -> Result<i32, i32> {
        let cfile = CString::new(filename).unwrap();
        let cmode: CString;
        let modeptr: *const i8 = if let Some(m) = mode {
            cmode = CString::new(m).unwrap();
            cmode.as_ptr()
        } else {
            std::ptr::null()
        };

        let r = unsafe {
            api::luaL_loadfilex(state, cfile.as_ptr(), modeptr)  
        };

        if r==0 {
            return Ok(0);
        }

        Err(r)
    }

    /// Creates and pushes a traceback of the stack `stack1`.
    ///
    /// If `msg` is not [None], it is appended at the beginning of the traceback.
    /// The level parameter tells at which level to start the traceback. 
    pub fn traceback(state: &lua_State, state1: &lua_State, msg: Option<&str>, level: i32) {
        if let Some(msgstr) = msg {
            let cmsg = CString::new(msgstr).unwrap();
            unsafe { api::luaL_traceback(state, state1, cmsg.as_ptr(), level) };
        } else {
            unsafe { api::luaL_traceback(state, state1, 0 as *const i8, level) };
        }
    }

    /// Registers all functions in the array `l` (see [crate::lua::luaL_Reg]) into the table
    /// on the top of the stack.
    ///
    /// When `nup` is not zero, all functions are created with `nup` upvalues,
    /// initialized with copies of the `nup` values previously pushed on the stack
    /// on top of the library table. These values are popped from the stack after
    /// the registration.
    ///
    /// A function with a [None] value represents a placeholder, which is filled
    /// with false. 
    pub fn setfuncs(state: &lua_State, l: &[crate::lua::luaL_Reg], nup: i32) {
        unsafe { api::luaL_setfuncs(state, l.as_ptr(), nup) }
    }

    /// Returns or creates the given metatable.
    ///
    /// If the registry already has the key `tname`, returns `0`. Otherwise,
    /// creates a new table to be used as a metatable for userdata, adds to this
    /// new table the pair __name = tname, adds to the registry the pair tname
    /// = new table, and returns 1.
    ///
    /// In both cases, the function pushes onto the stack the final value
    /// associated with tname in the registry. 
    pub fn newmetatable(state: &lua_State, tname: &str) -> bool {
        let ctname = CString::new(tname).unwrap();
        unsafe { api::luaL_newmetatable(state, ctname.as_ptr()) == 1 }
    }

    /// Checks whether the function argument `arg` is a userdata of the type
    /// `tname` (see [newmetatable]) and returns the userdata's memory-block
    /// address (see [crate::lua::touserdata]). 
    pub unsafe fn checkudata(state: &lua_State, ind: i32, tname: &str) -> *const std::ffi::c_void {
        let ctname = CString::new(tname).unwrap();
        unsafe { api::luaL_checkudata(state, ind, ctname.as_ptr()) }
    }

    /// Creates and returns a reference, in the table at index t, for the object
    /// on the top of the stack (and pops the object).
    ///
    /// A reference is a unique integer key. As long as you do not manually add
    /// integer keys into the table t, luaL_ref ensures the uniqueness of the
    /// key it returns. You can retrieve an object referred by the reference r
    /// by calling `lua_rawgeti(L, t, r)`. The function luaL_unref frees a
    /// reference.
    ///
    /// If the object on the top of the stack is nil, luaL_ref returns the
    /// constant LUA_REFNIL. The constant LUA_NOREF is guaranteed to be
    /// different from any reference returned by luaL_ref. 
    pub fn ref_(state: &lua_State, t: i32) -> i64 {
        unsafe { api::luaL_ref(state, t) }
    }

    /// Releases the reference ref from the table at index t (see luaL_ref).
    ///
    /// The entry is removed from the table, so that the referred object can be
    /// collected. The reference ref is also freed to be used again.
    ///
    /// If ref is LUA_NOREF or LUA_REFNIL, luaL_unref does nothing. 
    pub fn unref(state: &lua_State, t: i32, n: i64) {
        unsafe { api::luaL_unref(state, t, n) }
    }

    /// lauxlib FFI API
    mod api {
        use crate::lua::lua_State;

        #[link(name="lua-5.4.7")]
        unsafe extern "C" {
            pub fn luaL_checktype(state: &lua_State, arg: i32, t: i32);

            pub fn luaL_setfuncs(state: &lua_State, l: *const crate::lua::luaL_Reg, nup: i32);

            pub fn luaL_newstate() -> Option<&'static lua_State>;
            pub fn luaL_len(state: &lua_State, idx: i32) -> usize;
            pub fn luaL_openlibs(state: &lua_State);

            pub fn luaL_newmetatable(state: &lua_State, tname: *const i8) -> i32;

            pub fn luaL_checkstack(state: &lua_State, sz: i32, msg: *const i8);

            pub fn luaL_checkinteger(state: &lua_State, ind: i32) -> i64;
            pub fn luaL_checknumber(state: &lua_State, ind: i32) -> f64;
            pub fn luaL_checklstring(state: &lua_State, ind: i32, l: *mut usize) -> *const i8;
            pub fn luaL_checkudata(state: &lua_State, ind: i32, tname: *const i8) -> *const std::ffi::c_void;

            pub fn luaL_loadfilex(state: &lua_State, filename: *const i8, mode: *const i8) -> i32;

            pub fn luaL_traceback(L: &lua_State, L1: &lua_State, msg: *const i8, level: i32);

            pub fn luaL_ref(state: &lua_State, t: i32) -> i64;
            pub fn luaL_unref(state: &lua_State, t: i32, n: i64);
        }
    }
}

