/*** RST
jansson
=======

.. lua:module:: jansson

.. code-block:: lua

    local json = require 'jansson'

The :lua:mod:`jansson` Module provides JSON parsing, manipulation, and
serialization. It is a thin wrapper of the Jansson library.
*/

#include "lua-json.h"
#include <lauxlib.h>
#include "lua-manager.h"
#include <string.h>
#include "logging/logger.h"

int json_lua_open_module(lua_State *L);

void json_lua_init() {
    lua_manager_add_module_opener("jansson", &json_lua_open_module);
}

// module functions
int json_lua_mod_loads(lua_State *L);
int json_lua_mod_load_file(lua_State *L);
int json_lua_mod_dumps(lua_State *L);
int json_lua_mod_JSON_INDENT(lua_State *L);
int json_lua_mod_JSON_REAL_PRECISION(lua_State *L);
int json_lua_mod_array(lua_State *L);
int json_lua_mod_object(lua_State *L);

luaL_Reg json_mod_funcs[] = {
    "loads"              , &json_lua_mod_loads,
    "loadfile"           , &json_lua_mod_load_file,
    "dumps"              , &json_lua_mod_dumps,
    "array"              , &json_lua_mod_array,
    "object"             , &json_lua_mod_object,
    "JSON_INDENT"        , &json_lua_mod_JSON_INDENT,
    "JSON_REAL_PRECISION", &json_lua_mod_JSON_REAL_PRECISION,
    NULL                 ,  NULL
};

int json_lua_open_module(lua_State *L) {
    lua_newtable(L);
    luaL_setfuncs(L, json_mod_funcs, 0);

    // json_loads flags
    lua_pushinteger(L, JSON_REJECT_DUPLICATES);
    lua_setfield(L, -2, "JSON_REJECT_DUPLICATES");

    lua_pushinteger(L, JSON_DECODE_ANY);
    lua_setfield(L, -2, "JSON_DECODE_ANY");

    lua_pushinteger(L, JSON_DISABLE_EOF_CHECK);
    lua_setfield(L, -2, "JSON_DISABLE_EOF_CHECK");

    lua_pushinteger(L, JSON_DECODE_INT_AS_REAL);
    lua_setfield(L, -2, "JSON_DECODE_INT_AS_REAL");

    lua_pushinteger(L, JSON_ALLOW_NUL);
    lua_setfield(L, -2, "JSON_ALLOW_NUL");

    // json_dumps flags
    lua_pushinteger(L, JSON_COMPACT);
    lua_setfield(L, -2, "JSON_COMPACT");

    lua_pushinteger(L, JSON_ENSURE_ASCII);
    lua_setfield(L, -2, "JSON_ENSURE_ASCII");

    lua_pushinteger(L, JSON_SORT_KEYS);
    lua_setfield(L, -2, "JSON_SORT_KEYS");

    lua_pushinteger(L, JSON_ENCODE_ANY);
    lua_setfield(L, -2, "JSON_ENCODE_ANY");

    lua_pushinteger(L, JSON_ESCAPE_SLASH);
    lua_setfield(L, -2, "JSON_ESCAPE_SLASH");

    lua_pushinteger(L, JSON_EMBED);
    lua_setfield(L, -2, "JSON_EMBED");

    return 1;
}

/*** RST
Functions
---------

.. lua:function:: loads(jsonstring[, flags])

    Load JSON from a string.

    :param jsonstring: String to parse.
    :type jsonstring: string
    :param flags: (Optional) Load flags, see below.
    :type flags: integer
    :return: A parsed JSON
    :rtype: JSON.json

    **Load Flags**

    .. lua:data:: JSON_REJECT_DUPLICATES

        Issue a decoding error if any JSON object in the input text contains
        duplicate keys. Without this flag, the value of the last occurrence of
        each key ends up in the result. Key equivalence is checked byte-by-byte,
        without special Unicode comparison algorithms.

    .. lua:data:: JSON_DECODE_ANY

        By default, the decoder expects an array or object as the input. With
        this flag enabled, the decoder accepts any valid JSON value.

    .. lua:data:: JSON_DISABLE_EOF_CHECK

        By default, the decoder expects that its whole input constitutes a valid
        JSON text, and issues an error if there’s extra data after the otherwise
        valid JSON input. With this flag enabled, the decoder stops after
        decoding a valid JSON array or object, and thus allows extra data after
        the JSON text.

    .. lua:data:: JSON_DECODE_INT_AS_REAL

        JSON defines only one number type. Jansson distinguishes between ints
        and reals. For more information see Real vs. Integer. With this flag
        enabled the decoder interprets all numbers as real values. Integers that
        do not have an exact double representation will silently result in a
        loss of precision. Integers that cause a double overflow will cause an
        error.

    .. lua:data:: JSON_ALLOW_NUL

    .. code-block:: lua
        :caption: Example

        local jsonstr = [[
        {
            "Key": "value",
            "anotherKey": 1234
        }
        ]]

        -- with no flags
        local data = JSON.loads(jsonstr)

        -- with flags
        local data = JSON.loads(jsonstr, JSON.JSON_REJECT_DUPLICATES | JSON.JSON_DECODE_INT_AS_REAL)

    .. versionhistory::
        :0.0.1: Added
*/
int json_lua_mod_loads(lua_State *L) {
    const char *string = luaL_checkstring(L, 1);

    int flags = 0;

    if (lua_gettop(L)==2) flags = (int)luaL_checkinteger(L, 2);

    json_error_t error = {0};
    json_t *json = json_loads(string, flags, &error);

    if (!json) {
        return luaL_error(L, "%s:%d:%d - %s", error.source, error.line, error.column, error.text);
    } else {
        lua_pushjson(L, json);
        json_decref(json);
    }

    return 1;
}

/*** RST
.. lua:function:: loadfile(filename[, flags])

    Load JSON from a string.

    :param string filename:
    :param integer flags: (Optional) Load flags, see below.
    :return: A parsed JSON
    :rtype: JSON.json

    **Load Flags**

    .. lua:data:: JSON_REJECT_DUPLICATES

        Issue a decoding error if any JSON object in the input text contains
        duplicate keys. Without this flag, the value of the last occurrence of
        each key ends up in the result. Key equivalence is checked byte-by-byte,
        without special Unicode comparison algorithms.

    .. lua:data:: JSON_DECODE_ANY

        By default, the decoder expects an array or object as the input. With
        this flag enabled, the decoder accepts any valid JSON value.

    .. lua:data:: JSON_DISABLE_EOF_CHECK

        By default, the decoder expects that its whole input constitutes a valid
        JSON text, and issues an error if there’s extra data after the otherwise
        valid JSON input. With this flag enabled, the decoder stops after
        decoding a valid JSON array or object, and thus allows extra data after
        the JSON text.

    .. lua:data:: JSON_DECODE_INT_AS_REAL

        JSON defines only one number type. Jansson distinguishes between ints
        and reals. For more information see Real vs. Integer. With this flag
        enabled the decoder interprets all numbers as real values. Integers that
        do not have an exact double representation will silently result in a
        loss of precision. Integers that cause a double overflow will cause an
        error.

    .. lua:data:: JSON_ALLOW_NUL

    .. code-block:: lua
        :caption: Example

        local jsonstr = [[
        {
            "Key": "value",
            "anotherKey": 1234
        }
        ]]

        -- with no flags
        local data = JSON.loads(jsonstr)

        -- with flags
        local data = JSON.loads(jsonstr, JSON.JSON_REJECT_DUPLICATES | JSON.JSON_DECODE_INT_AS_REAL)

    .. versionhistory::
        :0.1.0: Added
*/
int json_lua_mod_load_file(lua_State *L) {
    const char *filename = luaL_checkstring(L, 1);

    int flags = 0;

    if (lua_gettop(L)==2) flags = (int)luaL_checkinteger(L, 2);

    json_error_t error = {0};
    json_t *json = json_load_file(filename, flags, &error);

    if (!json) {
        return luaL_error(L, "%s:%d:%d - %s", error.source, error.line, error.column, error.text);
    } else {
        lua_pushjson(L, json);
        json_decref(json);
    }

    return 1;
}

/*** RST
.. lua:function:: dumps(json[, flags])

    Serialize a JSON to a string.

    :param json: a :lua:class:`JSON.json`
    :type json: JSON.json
    :param flags: (Optional) Dump flags, see below.
    :type flags: integer
    :return: The serialized string
    :rtype: string

    **Dump Flags**

    .. lua:data:: JSON_INDENT(n)

        Pretty-print the result, using newlines between array and object items,
        and indenting with n spaces. The valid range for n is between 0 and 31
        (inclusive), other values result in an undefined output. If
        ``JSON_INDENT`` is not used or n is 0, no newlines are inserted between
        array and object items.

        The maximum indentation that can be used is 31.

    .. lua:data:: JSON_COMPACT

        This flag enables a compact representation, i.e. sets the separator
        between array and object items to ``","`` and between object keys and
        values to ``":"``. Without this flag, the corresponding separators are
        ``", "`` and ``": "`` for more readable output.

    .. lua:data:: JSON_ENSURE_ASCII

        If this flag is used, the output is guaranteed to consist only of ASCII
        characters. This is achieved by escaping all Unicode characters outside
        the ASCII range.
    
    .. lua:data:: JSON_SORT_KEYS

        If this flag is used, all the objects in output are sorted by key. This
        is useful e.g. if two JSON texts are diffed or visually compared.
    
    .. lua:data:: JSON_ENCODE_ANY

        Specifying this flag makes it possible to encode any JSON value on its
        own. Without it, only objects and arrays can be passed as the json value
        to the encoding functions.

    .. lua:data:: JSON_ESCAPE_SLASH

        Escape the ``/`` characters in strings with ``\/``.

    .. lua:data:: JSON_REAL_PRECISION(n)

        Output all real numbers with at most n digits of precision. The valid
        range for n is between 0 and 31 (inclusive), and other values result in
        an undefined behavior.

        By default, the precision is 17, to correctly and losslessly encode all
        IEEE 754 double precision floating point numbers.

    .. lua:data:: JSON_EMBED

        If this flag is used, the opening and closing characters of the
        top-level array (‘[’, ‘]’) or object (‘{’, ‘}’) are omitted during
        encoding. This flag is useful when concatenating multiple arrays or
        objects into a stream.

    .. versionhistory::
        :0.0.1: Added
*/
int json_lua_mod_dumps(lua_State *L) {
    json_t *json = lua_checkjson(L, 1);
    int flags = 0;
    if (lua_gettop(L)==2) flags = (int)luaL_checkinteger(L, 2);

    char *str = json_dumps(json, flags);
    lua_pushstring(L, str);
    free(str);

    return 1;    
}

/*** RST
.. lua:function:: array()

    Create a new empty JSON array.

    :rtype: JSON.json

    ..versionhistory::
        :0.1.0: Added
*/
int json_lua_mod_array(lua_State *L) {
    json_t *arr = json_array();

    lua_pushjson(L, arr);
    return 1;
}

/*** RST
.. lua:function:: object()

    Create a new empty JSON object.

    :rtype: JSON.json

    .. versionhistory::
        :0.1.0: Added
*/
int json_lua_mod_object(lua_State *L) {
    json_t *obj = json_object();

    lua_pushjson(L, obj);
    return 1;
}

int json_lua_mod_JSON_INDENT(lua_State *L) {
    int n = (int)luaL_checkinteger(L, 1);
    if (n < 0 || n > 31) {
        return luaL_error(L, "JSON_INDENT(n) - n must be between 0 and 31.");
    }

    lua_pushinteger(L, JSON_INDENT(n));

    return 1;
}

int json_lua_mod_JSON_REAL_PRECISION(lua_State *L) {
    int n = (int)luaL_checkinteger(L, 1);
    if (n < 0 || n > 31) {
        return luaL_error(L, "JSON_REAL_PRECISION(n) - n must be between 0 and 31.");
    }

    lua_pushinteger(L, JSON_REAL_PRECISION(n));

    return 1;
}

/*** RST
Classes
-------

.. lua:class:: json

    A JSON value. This is a wrapper around a Jansson ``json_t``, but it is
    intended to function as a Lua table or sequence. As such the only 
    :lua:class:`json` values a Lua user will enounter will be objects or arrays.

    All other values are converted automatically to or from native Lua types
    when they are accessed or set.

    .. code-block:: lua
        :caption: Example

        local json = JSON.loads('{"foo": "bar"}')

        -- 'bar'
        local foo = json.foo

        -- this is the same
        foo = json['foo']

        -- asignment works the same
        json['foo'] = 'Hello JSON!'
        json.foo = 'Hello JSON!'

        -- assignment change change the value types, too
        json.foo = 1234

        -- new keys are created by assigning to them
        json.bar = 'baz'

        -- JSON objects function nearly exactly the same as tables
        for k,v in pairs(json) do
            print(k .. ' = ' .. v)
        end

        -- arrays are similar but use integers for indexes
        json = JSON.loads('["foo", "bar", "baz"]')

        -- like Lua sequences, indexes start at 1
        foo = json[1] -- 'foo'

        -- and arrays also function like sequences
        for i,v in ipairs(json) do
            print(i .. ' = ' .. v)
        end

        -- including appending values
        table.insert(json, "Hello JSON!")

    .. versionhistory::
        :0.0.1: Added
*/


int json_lua_del(lua_State *L);
int json_lua_index(lua_State *L);
int json_lua_new_index(lua_State *L);
int json_lua_len(lua_State *L);
int json_lua_pairs(lua_State *L);
int json_lua_next(lua_State *L);

luaL_Reg json_funcs[] = {
    "__gc"      , &json_lua_del,
    "__index"   , &json_lua_index,
    "__newindex", &json_lua_new_index,
    "__len"     , &json_lua_len,
    "__pairs"   , &json_lua_pairs,
    NULL        ,  NULL
};

void lua_pushjson(lua_State *L, json_t *json) {
    switch(json_typeof(json)) {
    case JSON_TRUE   : lua_pushboolean(L, 1                       ); return;
    case JSON_FALSE  : lua_pushboolean(L, 0                       ); return;
    case JSON_NULL   : lua_pushnil    (L                          ); return;
    case JSON_INTEGER: lua_pushinteger(L, json_integer_value(json)); return;
    case JSON_REAL   : lua_pushnumber (L, json_real_value   (json)); return;
    case JSON_STRING : lua_pushstring (L, json_string_value (json)); return;
    }

    json_t **pjson = lua_newuserdata(L, sizeof(json_t*));
    *pjson = json;

    if (luaL_newmetatable(L, "jansson.json")) {
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
        luaL_setfuncs(L, json_funcs, 0);
    }
    lua_setmetatable(L, -2);

    json_incref(json);
}

json_t *lua_tojson(lua_State *L, int index) {
    switch(lua_type(L, index)) {
    case LUA_TUSERDATA: return json_deep_copy(lua_checkjson(L, index));
    case LUA_TNUMBER:   
        if (lua_isinteger(L, index)) return json_integer(luaL_checkinteger(L, index));
        else return json_real(luaL_checknumber(L, index));
    case LUA_TBOOLEAN:  return json_boolean(lua_toboolean(L, index));
    case LUA_TNIL:      return json_null();
    case LUA_TSTRING: {
        const char *val = lua_tostring(L, index);
        return json_string(val);
    }   
    default:
        luaL_error(L, "Lua type incompatible with JSON types.");
        return NULL;
    }
}

json_t *lua_checkjson(lua_State *L, int index) {
    return *(json_t**)luaL_checkudata(L, index, "jansson.json");
}

int json_lua_del(lua_State *L) {
    json_t *json = lua_checkjson(L, 1);
    
    json_decref(json);

    return 0;
}

int json_lua_index(lua_State *L) {
    json_t *json = lua_checkjson(L, 1);

    if (json_is_array(json)) {
        int ind = (int)luaL_checkinteger(L, 2);
        ind--; // lua indexes start at 1, we start at 0
        if (ind < 0 || ind >= json_array_size(json)) {
            // we will get an ind > size when iterating using ipairs in lua
            // in that case return nil
            lua_pushnil(L);
            return 1;
        }
        json_t *val = json_array_get(json, ind);
        lua_pushjson(L, val);
    } else if (json_is_object(json)) {
        const char *key = luaL_checkstring(L, 2);

        json_t *val = json_object_get(json, key);
        if (val) lua_pushjson(L, val);
        else lua_pushnil(L);
    } else {
        // this should never happen
        return luaL_error(L, "JSON object is not an array or an object, can't be indexed.");
    }

    return 1;
}

int json_lua_new_index(lua_State *L) {
    json_t *json = lua_checkjson(L, 1);

    if (json_is_array(json)) {
        int ind = (int)luaL_checkinteger(L, 2);
        ind--; // lua indexes start at 1, we start at 0
        if (ind < 0 || ind > json_array_size(json)) {
            return luaL_error(L, "JSON array index %d out of range.", ind);
        }
        json_t *val = lua_tojson(L, 3);
        if (ind == json_array_size(json)) json_array_append(json, val);
        else json_array_set(json, ind, val);
        json_decref(val);
    } else if (json_is_object(json)) {
        const char *key = luaL_checkstring(L, 2);

        json_t *val = lua_tojson(L, 3);
        json_object_set(json, key, val);
        json_decref(val);
    } else {
        // this should never happen
        return luaL_error(L, "JSON object is not an array or an object, can't be indexed.");
    }

    return 0;
}

int json_lua_len(lua_State *L) {
    json_t *json = lua_checkjson(L, 1);

    if (json_is_array(json)) {
        lua_pushinteger(L, json_array_size(json));
    } else if (json_is_object(json)) {
        lua_pushinteger(L, 0);
    } else {
        // this should never happen
        return luaL_error(L, "JSON object is not an array or an object, can't be indexed. (%d)", json_typeof(json));
    }

    return 1;
}

int json_lua_pairs(lua_State *L) {
    json_t *json = lua_checkjson(L, 1);

    /*if (json_is_array(json)) {
        lua_pushinteger(L, 0);
    } else */
    if (json_is_object(json)) {
        lua_pushcfunction(L, &json_lua_next);
        lua_pushvalue(L, 1);
        lua_pushnil(L);
    } else {
        // this should never happen
        return luaL_error(L, "JSON object is not an object, can't be used with pairs.");
    }

    return 3;
}

int json_lua_next(lua_State *L) {
    json_t *json = lua_checkjson(L, 1);

    if (json_is_array(json)) {
        int ind = (int)luaL_checkinteger(L, 2);
        
        if (ind+1==json_array_size(json)) { // if this ind is the last valid index
            lua_pushnil(L);
            return 1;
        }
        
        lua_pushinteger(L, ind+1);
        lua_pushjson(L, json_array_get(json, ind+1));
    } else if (json_is_object(json)) {
        const char *key;
        json_t *val;
        if (lua_type(L, 2)==LUA_TNIL) {
            key = json_object_iter_key(json_object_iter(json));

            if (key) {
                val = json_object_iter_value(json_object_key_to_iter(key));
                lua_pushstring(L, key);
                lua_pushjson(L, val);
                return 2;
            } else {
                lua_pushnil(L);
                return 1;
            }
        }

        const char *cur_key = luaL_checkstring(L, 2);
        int nextkey = 0;
        json_object_foreach(json, key, val) {
            if (nextkey) {
                lua_pushstring(L, key);
                lua_pushjson(L, val);
                return 2;
            }
            if (strcmp(key, cur_key)==0) nextkey = 1;            
        }
        lua_pushnil(L);
        return 1;
    } else {
        // this should never happen
        return luaL_error(L, "JSON object is not an array or an object, can't be indexed.");
    }

    return 0;
}
