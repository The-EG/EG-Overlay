// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT

//! SQLite3 Lua biding

/*** RST
SQLite3
=======


*/
use crate::lua;
use crate::lua::lua_State;
use crate::lua::luaL_Reg;
use crate::lua::luaL_Reg_list;

use std::ffi::{CString, CStr};

use crate::overlay::lua::luaerror;

use crate::logging::debug;

pub fn init() {
    debug!("Initializing SQLite...");
    if unsafe { api::sqlite3_initialize() } != api::SQLITE_OK {
        panic!("Can't initialize SQLite.");
    }
}

pub fn cleanup() {
    debug!("Shutting SQLite down...");
    unsafe { api::sqlite3_shutdown() };
}

fn get_db_err_msg(db: &api::sqlite3) -> String {
    let cerr = unsafe { CStr::from_ptr(api::sqlite3_errmsg(db)) };
    String::from(cerr.to_string_lossy())
}

fn get_stmt_err_msg(stmt: &api::sqlite3_stmt) -> String {
    let db = unsafe { api::sqlite3_db_handle(stmt) };

    get_db_err_msg(unsafe { &(*db)})
}

/*** RST
.. lua:class:: sqlite3db

    A SQLite3 database connection.

    Database connections are created using :lua:func:`eg-overlay.sqlite3open`.
*/
const SQLITE3_METATABLE_NAME: &str = "SQLite3";

const SQLITE3_FUNCS: &[luaL_Reg] = luaL_Reg_list!{
    c"__close", sqlite3_close,
    c"prepare", sqlite3_prepare,
    c"execute", sqlite3_execute,
};

pub unsafe extern "C" fn sqlite3_open(l: &lua_State) -> i32 {
    let nm = unsafe { lua::L::checkstring(l, 1) };
    let nmstr = CString::new(nm).unwrap();

    let mut db: *const api::sqlite3 = std::ptr::null();

    let r = unsafe { api::sqlite3_open(nmstr.as_ptr(), &mut db) };

    if r == api::SQLITE_OK {
        let lua_db: *mut *const api::sqlite3 = unsafe {
            std::mem::transmute(lua::newuserdatauv(l, std::mem::size_of::<*const api::sqlite3>(), 0))
        };

        unsafe { *lua_db = db; }

        if lua::L::newmetatable(l, SQLITE3_METATABLE_NAME) {
            lua::pushvalue(l, -1);
            lua::setfield(l, -2, "__index");

            lua::L::setfuncs(l, SQLITE3_FUNCS, 0);
        }
        lua::setmetatable(l, -2);

        return 1;
    } else {
        let cerr = unsafe { CStr::from_ptr(api::sqlite3_errstr(r)) };
        luaerror!(l, "Error opening SQLite database connection: {}", cerr.to_string_lossy());
        return 0;
    }
}

unsafe fn checksqlite3<'a>(l: &'a lua_State, ind: i32) -> &'a api::sqlite3 {
    let ptr: *mut *const api::sqlite3 = unsafe {
        std::mem::transmute(lua::L::checkudata(l, ind, SQLITE3_METATABLE_NAME))
    };

    if unsafe { *ptr }.is_null() {
        lua::pushstring(l, "sqlite3 already finalized.");
        unsafe { lua::error(l); }
    }

    unsafe { &(**ptr) }
}

unsafe extern "C" fn sqlite3_close(l: &lua_State) -> i32 {
    let ptr: *mut *const api::sqlite3 = unsafe {
        std::mem::transmute(lua::L::checkudata(l, 1, SQLITE3_METATABLE_NAME))
    };

    if unsafe { *ptr }.is_null() {
        // already closed before
        return 0;
    }

    let db = unsafe { &(**ptr) };

    unsafe { api::sqlite3_db_release_memory(db); }
    unsafe { api::sqlite3_close_v2(db); }

    unsafe { *ptr = std::ptr::null(); }

    return 0;
}

/*** RST
    .. lua:method:: prepare(sql)

        Prepare the given ``sql`` statement. An error will be raised if an error
        occurs during prepartion, otherwise this method returns a new
        :lua:class:`sqlite3stmt`.

        .. note::
            If the supplied ``sql`` statement can not be prepared, an error is
            logged and ``nil`` is returned instead.
        
        :param string sql:
        :rtype: sqlite3stmt

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn sqlite3_prepare(l: &lua_State) -> i32 {
    let db = unsafe { checksqlite3(l, 1) };
    let sql = unsafe { lua::L::checkstring(l, 2) };
    let sqlstr = CString::new(sql).unwrap();

    let mut stmt: *const api::sqlite3_stmt = std::ptr::null();

    let r = unsafe { api::sqlite3_prepare_v2(db, sqlstr.as_ptr(), -1, &mut stmt, 0 as *mut *const i8) };

    if r==api::SQLITE_OK {
        let lua_stmt: *mut *const api::sqlite3_stmt = unsafe {
            std::mem::transmute(lua::newuserdatauv(l, std::mem::size_of::<*const api::sqlite3_stmt>(), 0))
        };

        unsafe { *lua_stmt = stmt; }

        if lua::L::newmetatable(l, STMT_METATABLE_NAME) {
            lua::pushvalue(l, -1);
            lua::setfield(l, -2, "__index");

            lua::L::setfuncs(l, STMT_FUNCS, 0);
        }
        lua::setmetatable(l, -2);

        return 1;
    }
    let err = get_db_err_msg(db);
    luaerror!(l, "Error during prepare: {}", err);

    return 0;
}

/*** RST
    .. lua:method:: execute(sql)

        A convenience function that prepares, steps, and finalizes a statement
        for the given ``sql``.

        If the statement returns data it will be returned as a Lua table. If no
        data is returned but the statement is executed successfully, ``true`` is
        returned. If any error occurs, it will be logged and nothing (``nil``)
        is returned.
        
        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn sqlite3_execute(l: &lua_State) -> i32 {
    let db = unsafe { checksqlite3(l, 1) };
    let sql = unsafe { lua::L::checkstring(l, 2) };
    let sqlstr = CString::new(sql).unwrap();

    let mut stmt: *const api::sqlite3_stmt = std::ptr::null();

    let r = unsafe { api::sqlite3_prepare_v2(db, sqlstr.as_ptr(), -1, &mut stmt, 0 as *mut *const i8) };

    if r != api::SQLITE_OK {
        let err = get_db_err_msg(db);
        luaerror!(l, "Error during prepare: {}", err);
        return 0;
    }

    let r = unsafe { api::sqlite3_step(stmt) };
    if r == api::SQLITE_DONE {
        unsafe { api::sqlite3_finalize(stmt) };
        lua::pushboolean(l, true);
        return 1;
    }

    if r==api::SQLITE_ROW {
        // put the row into a table
        let colcount: i32 = unsafe { api::sqlite3_column_count(stmt) };
        lua::createtable(l, 0, colcount);

        for c in 0i32..colcount as i32 {
            let cname = unsafe { CStr::from_ptr(api::sqlite3_column_name(stmt, c)).to_string_lossy() };

            match unsafe { api::sqlite3_column_type(stmt, c) } {
                api::SQLITE_INTEGER => lua::pushinteger(l, unsafe { api::sqlite3_column_int64(stmt, c) }),
                api::SQLITE_FLOAT => lua::pushnumber(l, unsafe { api::sqlite3_column_double(stmt, c) }),
                api::SQLITE_TEXT => {
                    let cstr = unsafe { CStr::from_ptr(api::sqlite3_column_text(stmt, c)) };
                    lua::pushstring(l, &cstr.to_string_lossy());
                },
                api::SQLITE_BLOB => {
                    let len = unsafe { api::sqlite3_column_bytes(stmt, c) };
                    let bytes_ptr = unsafe { api::sqlite3_column_blob(stmt, c) as *const i8};
                    let bytes = unsafe { std::slice::from_raw_parts(bytes_ptr, len as usize) };
                    lua::pushbytes(l, bytes);
                },
                api::SQLITE_NULL => lua::pushnil(l),
                _ => {
                    luaerror!(l, "Invalid SQLite3 type.");
                    lua::pop(l,1);
                    return 0;
                }
            }

            lua::setfield(l, -2, &cname);
        }

        unsafe { api::sqlite3_finalize(stmt); }

        return 1;
    }

    let err = get_stmt_err_msg(unsafe { &*stmt} );
    luaerror!(l, "Error during statement step: {}", err);
    unsafe { api::sqlite3_finalize(stmt) };

    return 0;
}

/*** RST
.. lua:class:: sqlite3stmt
*/

const STMT_METATABLE_NAME: &str = "SQLite3Statement";

const STMT_FUNCS: &[luaL_Reg] = luaL_Reg_list!{
    c"__gc"    , stmt_finalize,
    c"__close" , stmt_finalize,
    c"finalize", stmt_finalize,
    c"reset"   , stmt_reset,
    c"bind"    , stmt_bind,
    c"step"    , stmt_step,
};

unsafe fn checkstmt<'a>(l: &'a lua_State, ind: i32) -> &'a api::sqlite3_stmt {
    let ptr: *mut *const api::sqlite3_stmt = unsafe {
        std::mem::transmute(lua::L::checkudata(l, ind, STMT_METATABLE_NAME))
    };

    if unsafe { *ptr }.is_null() {
        lua::pushstring(l, "statement has already been finalized.");
        unsafe { lua::error(l); }
    }

    unsafe { &(**ptr) }
}

unsafe extern "C" fn stmt_finalize(l: &lua_State) -> i32 {
    let ptr: *mut *const api::sqlite3_stmt = unsafe {
        std::mem::transmute(lua::L::checkudata(l, 1, STMT_METATABLE_NAME))
    };

    if unsafe { *ptr }.is_null() {
        // already finalized
        return 0;
    }

    let stmt = unsafe { &(**ptr) };
    
    unsafe { api::sqlite3_finalize(stmt) };

    unsafe { *ptr = std::ptr::null(); }

    return 0;
}

/*** RST
    .. lua:method:: bind(key, value[, blob])

        Set a statement parameter to given value.

        ``key`` can be either an integer or a string and must match the parameters
        specified when this statement was prepared.

        See `SQLite parameters <https://www.sqlite.org/lang_expr.html#varparam>`_.

        .. important::
            Number parameters begin at ``1``.

        :param key:
        :param value:
        :param boolean blob: (Optional) Bind the parameter as a BLOB instead of
            autodetecting the type.

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn stmt_bind(l: &lua_State) -> i32 {
    let stmt = unsafe { checkstmt(l, 1) };

    if lua::gettop(l)!=4 && lua::gettop(l)!=3 {
        lua::pushstring(l, "statement:bind takes 2 or 3 arguments.");
        return unsafe { lua::error(l) };
    }

    let blob = if lua::gettop(l)==4 {
        lua::toboolean(l, 4)
    } else {
        false
    };

    let c: i32;
    if lua::luatype(l, 2)==lua::LuaType::LUA_TNUMBER {
        c = unsafe { lua::L::checkinteger(l, 2) } as i32;
        if c < 1 || c > unsafe { api::sqlite3_bind_parameter_count(stmt) } {
            luaerror!(l, "Invalid parameter number: {}", c);
            return 0;
        }
    } else {
        let name = unsafe { lua::L::checkstring(l, 2) };
        let namestr = CString::new(name.as_str()).unwrap();

        c = unsafe { api::sqlite3_bind_parameter_index(stmt, namestr.as_ptr()) };
        if c==0 {
            luaerror!(l, "Invalid parameter name: {}", name);
            return 0;
        }
    }

    let r: i32;

    if blob {
        let data = lua::tobytes(l, 3);
        r = unsafe { api::sqlite3_bind_blob64(
            stmt,
            c,
            data.as_ptr() as *const std::ffi::c_void,
            data.len() as u64,
            api::SQLITE_TRANSIENT
        ) };
    } else {
        r = match lua::luatype(l, 3) {
            lua::LuaType::LUA_TNIL => unsafe { api::sqlite3_bind_null(stmt, c) },
            lua::LuaType::LUA_TNUMBER => {
                if lua::isinteger(l, 3) {
                    unsafe { api::sqlite3_bind_int64(stmt, c, lua::tointeger(l, 3)) }
                } else {
                    unsafe { api::sqlite3_bind_double(stmt, c, lua::tonumber(l, 3)) }
                }
            },
            lua::LuaType::LUA_TBOOLEAN => {
                let v: i64 = if lua::toboolean(l, 3) { 1 } else { 0 };
                unsafe { api::sqlite3_bind_int64(stmt, c, v) }
            },
            lua::LuaType::LUA_TSTRING => {
                let v = lua::tostring(l, 3);
                let vstr = CString::new(v.as_str()).unwrap();
                unsafe { api::sqlite3_bind_text64(
                    stmt,
                    c,
                    vstr.as_ptr(),
                    v.bytes().len() as u64,
                    api::SQLITE_TRANSIENT,
                    api::SQLITE_UTF8
                )}
            },
            _ => {
                luaerror!(l, "Couldn't bind Lua type.");
                return 0;
            }
        }
    }

    if r!=api::SQLITE_OK {
        luaerror!(l,"Couldn't bind parameter: {}", r);
        return 0;
    }

    return 0;
}

/*** RST
    .. lua:method:: step()

        Begin or continue execution of this statement.

        If the statement returns data, typically a single result row, it will
        be returned as a Lua table. If the statement successfully completes but
        no data is returned, true is returned. If an error occurs, it will be
        logged and nothing (``nil``) is returned.

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn stmt_step(l: &lua_State) -> i32 {
    let stmt = unsafe { checkstmt(l, 1) };

    let r = unsafe { api::sqlite3_step(stmt) };

    if r==api::SQLITE_DONE {
        lua::pushboolean(l, true);
        return 1;
    }

    if r==api::SQLITE_ROW {
        // put the row into a table
        let colcount: i32 = unsafe { api::sqlite3_column_count(stmt) };
        lua::createtable(l, 0, colcount);

        for c in 0i32..colcount as i32 {
            let cname = unsafe { CStr::from_ptr(api::sqlite3_column_name(stmt, c)).to_string_lossy() };

            match unsafe { api::sqlite3_column_type(stmt, c) } {
                api::SQLITE_INTEGER => lua::pushinteger(l, unsafe { api::sqlite3_column_int64(stmt, c) }),
                api::SQLITE_FLOAT => lua::pushnumber(l, unsafe { api::sqlite3_column_double(stmt, c) }),
                api::SQLITE_TEXT => {
                    let cstr = unsafe { CStr::from_ptr(api::sqlite3_column_text(stmt, c)) };
                    lua::pushstring(l, &cstr.to_string_lossy());
                },
                api::SQLITE_BLOB => {
                    let len = unsafe { api::sqlite3_column_bytes(stmt, c) };
                    let bytes_ptr = unsafe { api::sqlite3_column_blob(stmt, c) as *const i8};
                    let bytes = unsafe { std::slice::from_raw_parts(bytes_ptr, len as usize) };
                    lua::pushbytes(l, bytes);
                },
                api::SQLITE_NULL => lua::pushnil(l),
                _ => {
                    luaerror!(l, "Invalid SQLite3 type.");
                    lua::pop(l,1);
                    return 0;
                }
            }

            lua::setfield(l, -2, &cname);
        }

        return 1;
    }

    let err = get_stmt_err_msg(stmt);
    luaerror!(l, "Error during statement step: {}", err);

    return 0;
}

/*** RST
    .. lua:method:: reset()

        Resets this statement to an initial state, allowing it to be executed
        again.

        .. warning::
            Values bound with :lua:meth:`bind` are not cleared by this method.

        :rtype: string

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn stmt_reset(l: &lua_State) -> i32 {
    let stmt = unsafe { checkstmt(l, 1) };
    
    lua::pushstring(l, err_to_str(unsafe { api::sqlite3_reset(stmt) }));

    return 1;
}


fn err_to_str(err: i32) -> &'static str {
    match err {
        api::SQLITE_OK         => "ok",
        api::SQLITE_ERROR      => "error",
        api::SQLITE_INTERNAL   => "internal-error",
        api::SQLITE_PERM       => "permission-denied",
        api::SQLITE_ABORT      => "abort",
        api::SQLITE_BUSY       => "busy",
        api::SQLITE_LOCKED     => "locked",
        api::SQLITE_NOMEM      => "nonmem",
        api::SQLITE_READONLY   => "readonly",
        api::SQLITE_INTERRUPT  => "interrupt",
        api::SQLITE_IOERR      => "ioerr",
        api::SQLITE_CORRUPT    => "corrupt",
        api::SQLITE_NOTFOUND   => "notfound",
        api::SQLITE_FULL       => "full",
        api::SQLITE_CANTOPEN   => "cantopen",
        api::SQLITE_PROTOCOL   => "protocol",
        api::SQLITE_EMPTY      => "empty",
        api::SQLITE_SCHEMA     => "schema-changed",
        api::SQLITE_TOOBIG     => "too-big",
        api::SQLITE_CONSTRAINT => "constraint-violation",
        api::SQLITE_MISMATCH   => "mismatch",
        api::SQLITE_MISUSE     => "misusage",
        api::SQLITE_NOLFS      => "nolfs",
        api::SQLITE_AUTH       => "auth-denied",
        api::SQLITE_RANGE      => "out-of-range",
        api::SQLITE_NOTADB     => "not-a-db",
        api::SQLITE_NOTICE     => "notification",
        api::SQLITE_WARNING    => "warning",
        api::SQLITE_ROW        => "row",
        api::SQLITE_DONE       => "done",
        _                      => "unknown",
    }
}

mod api {
    use std::ffi::{c_int, c_char, c_void};

    #[repr(C)]
    pub struct sqlite3 {
        _foo: [u8;0],
    }

    #[repr(C)]
    pub struct sqlite3_stmt {
        _foo: [u8;0],
    }

    pub const SQLITE_OK: i32 = 0;

    pub const SQLITE_ERROR     : i32 =   1;
    pub const SQLITE_INTERNAL  : i32 =   2;
    pub const SQLITE_PERM      : i32 =   3;
    pub const SQLITE_ABORT     : i32 =   4;
    pub const SQLITE_BUSY      : i32 =   5;
    pub const SQLITE_LOCKED    : i32 =   6;
    pub const SQLITE_NOMEM     : i32 =   7;
    pub const SQLITE_READONLY  : i32 =   8;
    pub const SQLITE_INTERRUPT : i32 =   9;
    pub const SQLITE_IOERR     : i32 =  10;
    pub const SQLITE_CORRUPT   : i32 =  11;
    pub const SQLITE_NOTFOUND  : i32 =  12;
    pub const SQLITE_FULL      : i32 =  13;
    pub const SQLITE_CANTOPEN  : i32 =  14;
    pub const SQLITE_PROTOCOL  : i32 =  15;
    pub const SQLITE_EMPTY     : i32 =  16;
    pub const SQLITE_SCHEMA    : i32 =  17;
    pub const SQLITE_TOOBIG    : i32 =  18;
    pub const SQLITE_CONSTRAINT: i32 =  19;
    pub const SQLITE_MISMATCH  : i32 =  20;
    pub const SQLITE_MISUSE    : i32 =  21;
    pub const SQLITE_NOLFS     : i32 =  22;
    pub const SQLITE_AUTH      : i32 =  23;
    //pub const SQLITE_FORMAT    : i32 =  24;
    pub const SQLITE_RANGE     : i32 =  25;
    pub const SQLITE_NOTADB    : i32 =  26;
    pub const SQLITE_NOTICE    : i32 =  27;
    pub const SQLITE_WARNING   : i32 =  28;
    pub const SQLITE_ROW       : i32 =  100;
    pub const SQLITE_DONE      : i32 =  101;

    pub const SQLITE_TRANSIENT : *const c_void = -1i64 as *const c_void;

    pub const SQLITE_UTF8: u8 = 1;

    pub const SQLITE_INTEGER: c_int = 1;
    pub const SQLITE_FLOAT  : c_int = 2;
    pub const SQLITE_TEXT   : c_int = 3;
    pub const SQLITE_BLOB   : c_int = 4;
    pub const SQLITE_NULL   : c_int = 5;

    unsafe extern "C" {
        pub fn sqlite3_initialize() -> c_int;
        pub fn sqlite3_shutdown() -> c_int;

        pub fn sqlite3_db_release_memory(db: *const sqlite3) -> c_int;

        pub fn sqlite3_db_handle(stmt: *const sqlite3_stmt) -> *const sqlite3;

        pub fn sqlite3_open(filename: *const c_char, db: *mut *const sqlite3) -> c_int;
        pub fn sqlite3_close_v2(db: *const sqlite3) -> c_int;
        pub fn sqlite3_errstr(err: c_int) -> *const c_char;
        pub fn sqlite3_errmsg(db: *const sqlite3) -> *const c_char;

        pub fn sqlite3_prepare_v2(
            db: *const sqlite3,
            zSql: *const c_char,
            nByte: c_int,
            ppStmt: *mut *const sqlite3_stmt,
            pzTail: *mut *const c_char
        ) -> c_int;
        pub fn sqlite3_finalize(pStmt: *const sqlite3_stmt) -> c_int;
        pub fn sqlite3_reset(pStmt: *const sqlite3_stmt) -> c_int;
        pub fn sqlite3_step(pStmt: *const sqlite3_stmt) -> c_int;

        pub fn sqlite3_bind_parameter_count(pStmt: *const sqlite3_stmt) -> c_int;
        pub fn sqlite3_bind_parameter_index(pStmt: *const sqlite3_stmt, zName: *const c_char) -> c_int;

        pub fn sqlite3_bind_null(pStmt: *const sqlite3_stmt, param: c_int) -> c_int;
        pub fn sqlite3_bind_int64(pStmt: *const sqlite3_stmt, param: c_int, value: i64) -> c_int;
        pub fn sqlite3_bind_double(pStmt: *const sqlite3_stmt, param: c_int, value: f64) -> c_int;
        pub fn sqlite3_bind_blob64(
            pStmt: *const sqlite3_stmt,
            param: c_int,
            data: *const c_void,
            n: u64,
            dest: *const c_void
        ) -> c_int;
        pub fn sqlite3_bind_text64(
            pStmt: *const sqlite3_stmt,
            param: c_int,
            value: *const c_char,
            n: u64,
            dest: *const c_void,
            encoding: u8
        ) -> c_int;

        pub fn sqlite3_column_count(pStmt: *const sqlite3_stmt) -> c_int;
        pub fn sqlite3_column_bytes(pStmt: *const sqlite3_stmt, col: c_int) -> c_int;
        pub fn sqlite3_column_name(pStmt: *const sqlite3_stmt, n: c_int) -> *const c_char;
        pub fn sqlite3_column_type(pStmt: *const sqlite3_stmt, col: c_int) -> c_int;
        pub fn sqlite3_column_int64(pStmt: *const sqlite3_stmt, col: c_int) -> i64;
        pub fn sqlite3_column_double(pStmt: *const sqlite3_stmt, col: c_int) -> f64;
        pub fn sqlite3_column_text(pStmt: *const sqlite3_stmt, col: c_int) -> *const c_char;
        pub fn sqlite3_column_blob(pStmt: *const sqlite3_stmt, col: c_int) -> *const c_void;
    }
}
