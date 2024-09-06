/*** RST
sqlite
======

.. lua:module:: sqlite

.. code-block:: lua

    local sqlite = require 'sqlite'

The :lua:mod:`sqlite` module is a Lua binding for `SQLite3 <https://sqlite.org>`_.

Functions
---------
*/

#include <windows.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include <lua.h>
#include <lauxlib.h>
#include "logging/logger.h"

#include "lua-manager.h"

#define DBMTNAME "db"
#define STMTMTNAME "statement"

typedef struct db_t {
    sqlite3 *db;
} db_t;

typedef struct statement_t {
    db_t *db;
    sqlite3_stmt *stmt;
} statement_t;

int lua_open_db_module(lua_State *L);

#define luaL_checkdb(L, index) (db_t*)luaL_checkudata(L, index, DBMTNAME)
#define luaL_checkstatement(L, index) (statement_t*)luaL_checkudata(L, index, STMTMTNAME)

int sqlite_lua_memory_used(lua_State *L);
int sqlite_lua_memory_highwater(lua_State *L);

int db_lua_open(lua_State *L);
int db_lua_del(lua_State *L);
int db_lua_prepare(lua_State *L);
int db_lua_execute(lua_State *L);

int statement_lua_finalize(lua_State *L);
int statement_lua_bind(lua_State *L);
int statement_lua_step(lua_State *L);
int statement_lua_reset(lua_State *L);

luaL_Reg mod_funcs[] = {
    "open"            , &db_lua_open,
    "memory_used"     , &sqlite_lua_memory_used,
    "memory_highwater", &sqlite_lua_memory_highwater,
    NULL              , NULL
};

void lua_sqlite_init() {
    sqlite3_initialize();
    lua_manager_add_module_opener("sqlite", &lua_open_db_module);
}

int lua_open_db_module(lua_State *L) {
    lua_newtable(L);
    luaL_setfuncs(L, mod_funcs, 0);

    return 1;
}

luaL_Reg db_funcs[] = {
    "__gc"   , &db_lua_del,
    "prepare", &db_lua_prepare,
    "execute", &db_lua_execute,
    NULL     , NULL
};

luaL_Reg statment_funcs[] = {
    "__gc" , &statement_lua_finalize,
    "__close", &statement_lua_finalize,
    "finalize", &statement_lua_finalize,
    "bind" , &statement_lua_bind,
    "step" , &statement_lua_step,
    "reset", &statement_lua_reset,
    NULL   ,  NULL
};

/*** RST
.. lua:function:: open(db)

    Open the sqlite databse given by the string ``db``. This will usually be a
    path to a sqlite database, but it can be any valid SQLite database string.

    :rtype: sqlitedb

    .. versionhistory::
        :0.0.1: Added
*/
int db_lua_open(lua_State *L) {
    const char *name = luaL_checkstring(L, 1);

    sqlite3 *db = NULL;
    int err = sqlite3_open(name, &db);
    if (err!=SQLITE_OK) {
        const char *errstr = sqlite3_errstr(err);
        return luaL_error(L, "Couldn't open DB %s: %s", name, errstr);
    }

    db_t *lua_db = (db_t*)lua_newuserdata(L, sizeof(db_t));
    memset(lua_db, 0, sizeof(lua_db));
    lua_db->db = db;

    if (luaL_newmetatable(L, DBMTNAME)) {
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
        luaL_setfuncs(L, db_funcs, 0);
    }
    lua_setmetatable(L, -2);

    return 1;
}

/*** RST
.. lua:function:: memory_used()

    Return memory the SQLite has allocated in bytes.

    :rtype: integer

    .. versionhistory::
        :0.0.1: Added
*/
int sqlite_lua_memory_used(lua_State *L) {
    lua_pushinteger(L, sqlite3_memory_used());
    return 1;
}

/*** RST
.. lua:function:: memory_used()

    Return the maximum amount of memory SQLite has used since the overlay has
    started.

    :rtype: integer

    .. versionhistory::
        :0.0.1: Added
*/
int sqlite_lua_memory_highwater(lua_State *L) {
    int reset = 0;
    if (lua_gettop(L)==1) reset = lua_toboolean(L, 1);

    lua_pushinteger(L, sqlite3_memory_highwater(reset));
    return 1;
}

int db_lua_del(lua_State *L) {
    db_t *db = luaL_checkdb(L, 1);

    sqlite3_close_v2(db->db);

    return 0;
}

/*** RST
Classes
-------

.. lua:class:: sqlitedb

    A SQLite database connection.
*/

/*** RST
    .. lua:method:: prepare(sql)

        Prepare the given ``sql`` statement. An error will be raised if an error
        occurs during the prepare, otherwise this method returns a new
        :lua:class:`sqlitestatement`.

        :rtype: sqlitestatement

        .. versionhistory::
            :0.0.1: Added
*/
int db_lua_prepare(lua_State *L) {
    db_t *db = luaL_checkdb(L, 1);
    const char *sql = luaL_checkstring(L, 2);

    sqlite3_stmt *stmt = NULL;
    int r = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);

    if (r!=SQLITE_OK) {
        return luaL_error(L, "Couldn't prepare statement: %s", sqlite3_errmsg(db->db));
    }

    statement_t *dbstmt = lua_newuserdata(L, sizeof(statement_t));
    memset(dbstmt, 0, sizeof(statement_t));

    if (luaL_newmetatable(L, STMTMTNAME)) {
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
        luaL_setfuncs(L, statment_funcs, 0);
    }
    lua_setmetatable(L, -2);

    dbstmt->db = db;
    dbstmt->stmt = stmt;

    return 1;
}

/*** RST
    .. lua:method:: execute(sql)
    
        Execute the given ``sql`` statement and return the results, if any.

        This is a convenience method that calls :lua:meth:`prepare` then 
        :lua:meth:`sqlitestatement.step` to get all results and then
        :lua:meth:`sqlitestatement.finalize` before returning the
        results.

        :rtype: table

        .. versionhistory::
            :0.0.1: Added
*/
int db_lua_execute(lua_State *L) {
    db_t *db = luaL_checkdb(L, 1);
    const char *sql = luaL_checkstring(L, 2);

    sqlite3_stmt *stmt = NULL;
    int r = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);

    if (r!=SQLITE_OK) {
        return luaL_error(L, "Couldn't prepare statement: %s", sqlite3_errmsg(db->db));
    }

    r = sqlite3_step(stmt);
    if (r==SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return 0;
    }

    if (r==SQLITE_ROW) {
        // put the row into a table
        lua_newtable(L);
        for (int c=0;c<sqlite3_column_count(stmt);c++) {
            const char *cname = sqlite3_column_name(stmt, c);

            switch (sqlite3_column_type(stmt, c)) {
            case SQLITE_INTEGER:
                lua_pushinteger(L, sqlite3_column_int64(stmt, c));
                break;
            case SQLITE_FLOAT:
                lua_pushnumber(L, sqlite3_column_double(stmt, c));
                break;
            case SQLITE_TEXT:
                lua_pushstring(L, (const char *)sqlite3_column_text(stmt, c));
                break;
            case SQLITE_BLOB:
                lua_pushlstring(L, sqlite3_column_blob(stmt, c), sqlite3_column_bytes(stmt, c));
                break;
            case SQLITE_NULL:
                lua_pushnil(L);
                break;
            default:
                return luaL_error(L, "Invalid sqlite3 type: %d", sqlite3_column_type(stmt, c));
            }
            lua_setfield(L, -2, cname);
        }
        sqlite3_finalize(stmt);
        return 1;
    }

    sqlite3_finalize(stmt);

    return luaL_error(L, "Error during statement:step : (%d) %s", r, sqlite3_errmsg(db->db));
}

/*** RST
.. lua:class:: sqlitestatement

    A SQLite statement. Statements are prepared SQL statements that can be 
    ran against the database. They can container placeholders that can be set
    using :lua:meth:`bind`. See `SQLite parameters <https://www.sqlite.org/lang_expr.html#varparam>`_.

*/

/*** RST
    .. lua:method:: bind(key, value[, blob])

        Set the statement parameter to the given value.

        ``key`` can be either an integer or a string and much match the
        parameters specified when this statement was created.
        See `SQLite parameters <https://www.sqlite.org/lang_expr.html#varparam>`_.

        .. important::
            Numbered parameters begin with ``1``

        :param key:
        :param value:
        :param boolean blob: (Optional) Bind the parameter as a BLOB instead of
            autodetecting type.

        .. versionhistory::
            :0.0.1: Added
            :0.2.0: Added ``blob`` argument
*/
int statement_lua_bind(lua_State *L) {
    statement_t *stmt = luaL_checkstatement(L, 1);

    if (lua_gettop(L)!=4 && lua_gettop(L)!=3) return luaL_error(L, "statement:bind takes 2 or 3 arguments, name/index, value, and (optionally) blob.");

    int blob = lua_toboolean(L, 4);

    int c = 0;
    if (lua_type(L, 2)==LUA_TNUMBER) {
        c = (int)luaL_checkinteger(L, 2);
        if (c < 1 || c > sqlite3_bind_parameter_count(stmt->stmt)) 
            return luaL_error(L, "Invalid parameter number: %d", c);
    } else {
        const char *name = luaL_checkstring(L, 2);
        c = sqlite3_bind_parameter_index(stmt->stmt, name);
        if (c==0) return luaL_error(L, "Invalid parameter name: %s", name);
    }

    int r = 0;

    if (blob) {
        int len = 0;
        const char *data = lua_tolstring(L, 3, &len);
        sqlite3_bind_blob(stmt->stmt, c, data, len, SQLITE_TRANSIENT);
    } else {
        switch (lua_type(L, 3)) {
        case LUA_TNIL:
            r = sqlite3_bind_null(stmt->stmt, c);
            break;
        case LUA_TNUMBER:
            if (lua_isinteger(L, 3)) r = sqlite3_bind_int64(stmt->stmt, c, lua_tointeger(L, 3));
            else r = sqlite3_bind_double(stmt->stmt, c, lua_tonumber(L, 3));
            break;
        case LUA_TBOOLEAN:
            r = sqlite3_bind_int(stmt->stmt, c, lua_toboolean(L, 3));
            break;
        case LUA_TSTRING:
            r = sqlite3_bind_text(stmt->stmt, c, lua_tostring(L, 3), -1, SQLITE_TRANSIENT);
            break;
        default:
            return luaL_error(L, "Can't bind Lua type %d", lua_type(L,3));
        }
    }

    if (r!=SQLITE_OK)
        return luaL_error(L, "Couldn't bind parameter: %s", sqlite3_errmsg(stmt->db->db));

    return 0;
}

/*** RST
    .. lua:method:: step()

        Step this statement.
        
        The first time this function is called it will
        execute this statement. If the statement returns data, the first row
        will be returned. Remaining rows can be returned by calling this
        function until it returns ``nil``.

        If an error occurs during execution a Lua error will be raised.

        :rtype: table

        .. code-block:: lua
            :caption: Example

            local db = sqlite.open('foo.db')

            local stmt = db:prepare("SELECT * FROM atable WHERE id = ?")
            stmt:bind(1, 1234) -- bind 1234 to the first parameter (id)

            local results = {}

            -- step can be wrapped in an iterator
            local function allrows()
                return stmt:step()
            end

            -- which can be used with for
            for row in allrows do
                table.insert(results, row)
            end

            stmt:finalize()
            
        .. versionhistory::
            :0.0.1: Added
*/
int statement_lua_step(lua_State *L) {
    statement_t *stmt = luaL_checkstatement(L, 1);

    int r = sqlite3_step(stmt->stmt);
    if (r==SQLITE_DONE) return 0;

    if (r==SQLITE_ROW) {
        // put the row into a table
        lua_newtable(L);
        for (int c=0;c<sqlite3_column_count(stmt->stmt);c++) {
            const char *cname = sqlite3_column_name(stmt->stmt, c);

            switch (sqlite3_column_type(stmt->stmt, c)) {
            case SQLITE_INTEGER:
                lua_pushinteger(L, sqlite3_column_int64(stmt->stmt, c));
                break;
            case SQLITE_FLOAT:
                lua_pushnumber(L, sqlite3_column_double(stmt->stmt, c));
                break;
            case SQLITE_TEXT:
                lua_pushstring(L, (const char *)sqlite3_column_text(stmt->stmt, c));
                break;
            case SQLITE_BLOB:
                lua_pushlstring(L, sqlite3_column_blob(stmt->stmt, c), sqlite3_column_bytes(stmt->stmt, c));
                break;
            case SQLITE_NULL:
                lua_pushnil(L);
                break;
            default:
                return luaL_error(L, "Invalid sqlite3 type: %d", sqlite3_column_type(stmt->stmt, c));
            }

            lua_setfield(L, -2, cname);
        }

        return 1;
    }

    return luaL_error(L, "Error during statement:step : (%d) %s", r, sqlite3_errmsg(stmt->db->db));
}

/*** RST
    .. lua:method:: reset()

        Reset the statement so it can executed again.

        .. important::
            Resetting a statement does not clear out parameter values. Parameter
            values will remain set to the last value.

        .. versionhistory::
            :0.0.1: Added
*/
int statement_lua_reset(lua_State *L) {
    statement_t *stmt = luaL_checkstatement(L, 1);

    int r = sqlite3_reset(stmt->stmt);
    if (r!=SQLITE_OK) return luaL_error(L, "Error during statement:reset : (%d) %s", r, sqlite3_errmsg(stmt->db->db));

    return 0;
}

/*** RST
    .. lua:method:: finalize()

        Finalize this statement.

        .. important::
            This destroys the underlying SQLite statement and will automatically
            be called by Lua when the Lua variable holding this statement goes
            out of scope or is otherwise garbage collected.

            Module authors may wish to call this manually in certain cases, such
            as when manually using transactions.

        .. versionhistory::
            :0.0.1: Added
*/
int statement_lua_finalize(lua_State *L) {
    statement_t *stmt = luaL_checkstatement(L, 1);

    if (stmt->stmt) {
        sqlite3_finalize(stmt->stmt);
        stmt->stmt = NULL;
    }

    return 0;
}
