#include <stdlib.h>
#include <string.h>
#include "settings.h"
#include "logging/logger.h"
#include "utils.h"
#include "lua-manager.h"
#include "lua-json.h"
#include <lua.h>
#include <lauxlib.h>
#include <windows.h>

typedef enum settings_default_type_t {
    SETTINGS_DEFAULT_TYPE_JSON,
    SETTINGS_DEFAULT_TYPE_INT,
    SETTINGS_DEFAULT_TYPE_DOUBLE,
    SETTINGS_DEFAULT_TYPE_STRING,
    SETTINGS_DEFAULT_TYPE_BOOLEAN
} settings_default_type_t;

typedef struct settings_default_t {
    settings_default_type_t type;

    union {
        json_t *value_json;
        int value_int; // and bool
        double value_double;
        char *value_str;
    };
} settings_default_t;

struct settings_t {
    logger_t *log;

    uint8_t save_on_set;

    char *file_path;
    json_t *data;

    HANDLE mutex;

    // default value hashmap
    size_t default_values_hash_size;
    char **default_keys;
    settings_default_t **default_values;

    int ref_count;
};

void settings_free(settings_t *settings);

void settings_lock(settings_t *settings);
void settings_release(settings_t *settings);

json_t *settings_data_for_path(const char *path, json_t *data, int create);

uint32_t settings_get_default_key_ind(settings_t *settings, const char *key);

int settings_lua_new(lua_State *L);
int settings_lua_del(lua_State *L);
int settings_lua_set(lua_State *L);
int settings_lua_get(lua_State *L);
int settings_lua_set_default(lua_State *L);
int settings_lua_save_on_set(lua_State *L);
int settings_lua_save(lua_State *L);

settings_t *settings_new(const char *name) {
    CreateDirectory("settings", NULL);

    settings_t *settings = egoverlay_calloc(1, sizeof(settings_t));
    settings->log = logger_get("settings");
    
    settings->mutex = CreateMutex(NULL, 0, NULL);

    settings->file_path  = egoverlay_calloc(strlen(name) + strlen("settings/.json") + 1, sizeof(char));
    memcpy(settings->file_path , "settings/", strlen("settings/"));
    memcpy(settings->file_path  + strlen("settings/"), name, strlen(name));
    memcpy(settings->file_path  + strlen("settings/") + strlen(name), ".json", strlen(".json"));

    logger_info(settings->log, "Loading %s...", settings->file_path);

    json_error_t error = {0};
    settings->data = json_load_file(settings->file_path, 0, &error);

    if (!settings->data) {
        enum json_error_code errcode = json_error_code(&error);
        if (errcode==json_error_cannot_open_file) {
            logger_warn(settings->log, "Couldn't load %s: %s", settings->file_path, error.text);
            logger_warn(settings->log, "Creating new settings file: %s", settings->file_path);

            settings->data = json_object();
            settings_save(settings);
        } else {
            logger_error(
                settings->log,
                "Couldn't load %s: %s at %d:%d",
                settings->file_path,
                error.text,
                error.line,
                error.column
            );
            error_and_exit(
                "EG-Overlay: Settings Error",
                "Couldn't load %s:\n%s\nat %d:%d",
                settings->file_path,
                error.text,
                error.line,
                error.column
            );
        }
    }

    settings->default_values_hash_size = 64;
    settings->default_keys = egoverlay_calloc(settings->default_values_hash_size, sizeof(char*));
    settings->default_values = egoverlay_calloc(settings->default_values_hash_size, sizeof(settings_default_t*));

    settings->save_on_set = 1;

    settings_ref(settings);

    return settings;
}

void settings_ref(settings_t *settings) {
    settings->ref_count++;
}

void settings_unref(settings_t *settings) {
    settings->ref_count--;
    if (settings->ref_count==0) settings_free(settings);
}

void settings_free(settings_t *settings) {
    for (size_t h=0;h<settings->default_values_hash_size;h++) {
        if (settings->default_keys[h]) {
            egoverlay_free(settings->default_keys[h]);
            if (settings->default_values[h]->type==SETTINGS_DEFAULT_TYPE_STRING) {
                egoverlay_free(settings->default_values[h]->value_str);
            }
            if (settings->default_values[h]->type==SETTINGS_DEFAULT_TYPE_JSON) {
                json_decref(settings->default_values[h]->value_json);
            }
            egoverlay_free(settings->default_values[h]);
        }
    }
    egoverlay_free(settings->default_keys);
    egoverlay_free(settings->default_values);

    CloseHandle(settings->mutex);
    egoverlay_free(settings->file_path);
    json_decref(settings->data);

    egoverlay_free(settings);
}

int settings_open_lua_module(lua_State *L) {
    lua_newtable(L);
    lua_pushcfunction(L, &settings_lua_new);
    lua_setfield(L, -2, "new");

    return 1;
}

void settings_lua_init() {
    lua_manager_add_module_opener("settings", &settings_open_lua_module);
}

int settings_has_value(settings_t *settings, const char *key) {
    settings_lock(settings);

    json_t *val = NULL;
    int ret = settings_get(settings, key, &val);

    settings_release(settings);

    return ret;
}

int settings_get_internal(settings_t *settings, const char *key, json_t **value) {
    settings_lock(settings);

    json_t *val = settings_data_for_path(key, settings->data, 0);

    settings_release(settings);

    if (val) {
        *value = val;
        return 1;
    }

    *value = NULL;
    return 0;
}

int settings_get(settings_t *settings, const char *key, json_t **value) {
    int r = settings_get_internal(settings, key, value);

    if (!*value) {
        uint32_t key_ind = settings_get_default_key_ind(settings, key);
        if (settings->default_keys[key_ind] && strcmp(settings->default_keys[key_ind], key)==0) {
            settings_default_t *dv = settings->default_values[key_ind];
            if (dv->type==SETTINGS_DEFAULT_TYPE_JSON) {
                *value = dv->value_json;
                return 1;
            }
        }
    }
    
    logger_warn(settings->log, "[%s] GET UNKNOWN KEY %s", settings->file_path, key);
    return r;
}

int settings_get_int(settings_t *settings, const char *key, int *value) {
    json_t *val = NULL;
    settings_get_internal(settings, key, &val);

    if (val && json_is_integer(val)) {
        *value = (int)json_integer_value(val);
        return 1;
    } else if (!val) {
        uint32_t key_ind = settings_get_default_key_ind(settings, key);
        if (settings->default_keys[key_ind] && strcmp(settings->default_keys[key_ind], key)==0) {
            settings_default_t *dv = settings->default_values[key_ind];
            if (dv->type==SETTINGS_DEFAULT_TYPE_INT) {
                *value = dv->value_int;
                return 1;
            }
        }
    }
    
    logger_warn(settings->log, "[%s] GET UNKNOWN KEY %s", settings->file_path, key);

    return 0;
}

int settings_get_string(settings_t *settings, const char *key, char const **value) {
    json_t *val = NULL;
    settings_get_internal(settings, key, &val);

    if (val && json_is_string(val)) {
        const char *strval = json_string_value(val);
        if (strval) {
            *value = strval;

            return 1;
        }
    } else if (!val) {
        uint32_t key_ind = settings_get_default_key_ind(settings, key);
        if (settings->default_keys[key_ind] && strcmp(settings->default_keys[key_ind], key)==0) {
            settings_default_t *dv = settings->default_values[key_ind];
            if (dv->type==SETTINGS_DEFAULT_TYPE_STRING) {
                *value = dv->value_str;
                return 1;
            }
        }
    }

    logger_warn(settings->log, "[%s] GET UNKNOWN KEY %s", settings->file_path, key);

    *value = NULL;
    return 0;
}

int settings_get_double(settings_t *settings, const char *key, double *value) {
    json_t *val = NULL;
    settings_get_internal(settings, key, &val);

    if (val && json_is_real(val)) {
        *value = json_real_value(val);
        return 1;
    } else if (!val) {
        uint32_t key_ind = settings_get_default_key_ind(settings, key);
        if (settings->default_keys[key_ind] && strcmp(settings->default_keys[key_ind], key)==0) {
            settings_default_t *dv = settings->default_values[key_ind];
            if (dv->type==SETTINGS_DEFAULT_TYPE_DOUBLE) {
                *value = dv->value_double;
                return 1;
            }
        }
    }
    
    logger_warn(settings->log, "[%s] UNKNOWN KEY %s", settings->file_path, key);

    return 0;
}

int settings_get_boolean(settings_t *settings, const char *key, int *value) {
    json_t *val = NULL;
    settings_get_internal(settings, key, &val);

    if (val && json_is_boolean(val)) {
        *value = json_boolean_value(val);
        return 1;
    } else if (!val) {
        uint32_t key_ind = settings_get_default_key_ind(settings, key);
        if (settings->default_keys[key_ind] && strcmp(settings->default_keys[key_ind], key)==0) {
            settings_default_t *dv = settings->default_values[key_ind];
            if (dv->type==SETTINGS_DEFAULT_TYPE_BOOLEAN) {
                *value = dv->value_int;
                return 1;
            }
        }
    } 
    
    logger_warn(settings->log, "[%s] GET UNKNOWN KEY %s", settings->file_path, key);    

    return 0;
}

void settings_set_internal(settings_t *settings, const char *key, json_t *value) {
    size_t keylen = strlen(key);
    if (keylen==0 || key[0]=='.') return;

    settings_lock(settings);
    size_t last_dot = 0;
    
    for (size_t i=strlen(key)-1;i>0;i--) {
        if (key[i]=='.') {
            last_dot = i;
            break;
        }
    }

    json_t *parent = settings->data;
    if (last_dot) {
        char *parent_path = egoverlay_calloc(last_dot+1, sizeof(char));
        memcpy(parent_path, key, last_dot);

        parent = settings_data_for_path(parent_path, settings->data, 1);
        egoverlay_free(parent_path);
    }

    char *node_key = NULL;
    if (last_dot) {
        size_t node_key_len = keylen - last_dot;
        node_key = egoverlay_calloc(node_key_len + 1, sizeof(char));
        memcpy(node_key, key + last_dot + 1, node_key_len);
    } else {
        node_key = egoverlay_calloc(keylen + 1, sizeof(char));
        memcpy(node_key, key, keylen);
    }
    
    json_object_set(parent, node_key, value);
    egoverlay_free(node_key);

    if (settings->save_on_set) settings_save(settings);    

    settings_release(settings);
}

void settings_set(settings_t *settings, const char *key, json_t *value) {
    //logger_debug(settings->log, "[%s] SET %s -> (JSON object)", settings->file_path, key);
    settings_set_internal(settings, key, value);
}

void settings_set_int(settings_t *settings, const char *key, int value) {
    //logger_debug(settings->log, "[%s] SET %s <- %d", settings->file_path, key, value);
    json_t *val = json_integer(value);
    settings_set_internal(settings, key, val);
    json_decref(val);
}

void settings_set_string(settings_t *settings, const char *key, const char *value) {
    //logger_debug(settings->log, "[%s] SET %s <- %s", settings->file_path, key, value);

    json_t *val = json_string(value);
    settings_set_internal(settings, key, val);
    json_decref(val);
}

void settings_set_double(settings_t *settings, const char *key, double value) {
    //logger_debug(settings->log, "[%] SET %s <- %f", settings->file_path, key, value);
    json_t *val = json_real(value);
    settings_set_internal(settings, key, val);
    json_decref(val);
}

void settings_set_boolean(settings_t *settings, const char *key, int value) {
    //logger_debug(settings->log, "[%] SET %s <- %s", settings->file_path, key, value ? "true" : "false");
    json_t *val = json_boolean(value);
    settings_set_internal(settings, key, val);
    json_decref(val);
}

json_t *settings_data_for_path(const char *path, json_t *data, int create) {
    size_t pathlen = strlen(path);
    if (pathlen==0 || path[0]=='.') return NULL;

    if (!json_is_object(data)) return NULL;

    // first determine if path is a single node ie "some_value" or a 
    // larger path like "some_category.some_value"
    size_t first_dot = 0;
    for (size_t i=0;i<pathlen;i++) {
        if (path[i]=='.') {
            first_dot = i;
            break;
        }
    }

    if (first_dot==0) {
        // this is a single node path
        
        // if create flag is set first see if the key already exists
        // if so just return the value. if not, create it as an object
        json_t *val = json_object_get(data, path);

        if (create && !val) {
            val = json_object();
            json_object_set(data, path, val);
            json_decref(val);
        }

        return val;
    }

    // otherwise look for the first node in the path and then recurs
    char *first_node = egoverlay_calloc(first_dot + 1, sizeof(char));
    memcpy(first_node, path, first_dot);

    json_t *node_data = json_object_get(data, first_node);

    if (node_data) {
        egoverlay_free(first_node);
        return settings_data_for_path(path + first_dot + 1, node_data, create);
    }

    // setting key doesn't exist
    if (create) {
        json_t *val = json_object();
        json_object_set(data, first_node, val);
        json_decref(val);
        egoverlay_free(first_node);
        return settings_data_for_path(path + first_dot + 1, val, 1);
    }    
    egoverlay_free(first_node);
    return NULL;
}

void settings_lock(settings_t *settings) {
    WaitForSingleObject(settings->mutex, INFINITE);
}

void settings_release(settings_t *settings) {
    ReleaseMutex(settings->mutex);
}

// this one just finds the key_ind
uint32_t settings_get_default_key_ind(settings_t *settings, const char *key) {
    uint32_t key_hash = djb2_hash_string(key);
    uint32_t key_ind = key_hash % settings->default_values_hash_size;

    while (settings->default_keys[key_ind]!=NULL) {
        if (settings->default_keys[key_ind] && strcmp(settings->default_keys[key_ind], key)==0) break; // match
        key_ind++;
        if (key_ind>=settings->default_values_hash_size) key_ind = 0;
        if (key_ind==key_hash % settings->default_values_hash_size) {
            logger_error(settings->log, "Default value hash map full.");
            error_and_exit("EG-Overlay: Settings", "Default value hash map full.");
        }
    }

    return key_ind;
}

// this one finds the key_ind and creates the new key string
uint32_t settings_get_default_key_ind_and_create(settings_t *settings, const char *key) {
    uint32_t key_ind = settings_get_default_key_ind(settings, key);

    if (settings->default_keys[key_ind]==NULL) {
        settings->default_keys[key_ind] = egoverlay_calloc(strlen(key)+1, sizeof(char));
        memcpy(settings->default_keys[key_ind], key, strlen(key));
    }

    return key_ind;
}

void settings_free_default_value(settings_default_t *dv) {
    if (dv->type==SETTINGS_DEFAULT_TYPE_JSON) json_decref(dv->value_json);
    if (dv->type==SETTINGS_DEFAULT_TYPE_STRING) egoverlay_free(dv->value_str);
}

void settings_set_default(settings_t *settings, const char *key, json_t *value) {
    uint32_t key_ind = settings_get_default_key_ind_and_create(settings, key);

    if (settings->default_values[key_ind]) {
        settings_free_default_value(settings->default_values[key_ind]);
    } else {
        settings->default_values[key_ind] = egoverlay_calloc(1, sizeof(settings_default_t));

    }
    settings->default_values[key_ind]->type = SETTINGS_DEFAULT_TYPE_JSON;
    settings->default_values[key_ind]->value_json = value;
}

void settings_set_default_int(settings_t *settings, const char *key, int value) {
    uint32_t key_ind = settings_get_default_key_ind_and_create(settings, key);

    if (settings->default_values[key_ind]) {
        settings_free_default_value(settings->default_values[key_ind]);
    } else {
        settings->default_values[key_ind] = egoverlay_calloc(1, sizeof(settings_default_t));

    }
    settings->default_values[key_ind]->type = SETTINGS_DEFAULT_TYPE_INT;
    settings->default_values[key_ind]->value_int = value;
}

void settings_set_default_string(settings_t *settings, const char *key, const char *value) {
    uint32_t key_ind = settings_get_default_key_ind_and_create(settings, key);

    if (settings->default_values[key_ind]) {
        settings_free_default_value(settings->default_values[key_ind]);
    } else {
        settings->default_values[key_ind] = egoverlay_calloc(1, sizeof(settings_default_t));

    }
    settings->default_values[key_ind]->type = SETTINGS_DEFAULT_TYPE_STRING;
    settings->default_values[key_ind]->value_str = egoverlay_calloc(strlen(value)+1, sizeof(char));
    memcpy(settings->default_values[key_ind]->value_str, value, strlen(value));
}

void settings_set_default_double(settings_t *settings, const char *key, double value) {
    uint32_t key_ind = settings_get_default_key_ind_and_create(settings, key);

    if (settings->default_values[key_ind]) {
        settings_free_default_value(settings->default_values[key_ind]);
    } else {
        settings->default_values[key_ind] = egoverlay_calloc(1, sizeof(settings_default_t));

    }
    settings->default_values[key_ind]->type = SETTINGS_DEFAULT_TYPE_DOUBLE;
    settings->default_values[key_ind]->value_double = value;
}

void settings_set_default_boolean(settings_t *settings, const char *key, int value) {
    uint32_t key_ind = settings_get_default_key_ind_and_create(settings, key);

    if (settings->default_values[key_ind]) {
        settings_free_default_value(settings->default_values[key_ind]);
    } else {
        settings->default_values[key_ind] = egoverlay_calloc(1, sizeof(settings_default_t));

    }
    settings->default_values[key_ind]->type = SETTINGS_DEFAULT_TYPE_BOOLEAN;
    settings->default_values[key_ind]->value_int = value;
}

void settings_save(settings_t *settings) {
    //logger_debug(settings->log, "Writing to %s", settings->file_path);
    char *settings_str = json_dumps(settings->data, JSON_INDENT(2));

    FILE *settings_file = fopen(settings->file_path, "wb");
    fwrite(settings_str, sizeof(char), strlen(settings_str), settings_file);
    fflush(settings_file);
    fclose(settings_file);
    free(settings_str);
}

/*** RST
settings
========

.. lua:module:: settings

.. code:: lua

    local settings = require 'settings'

The :lua:mod:`settings` module can be used to create and read settings files,
which are stored in the ``settings`` folder as JSON files. Modules can have
multiple settings objects, and therefore files.

Functions
---------

.. lua:function:: new(name)

    Creates a new :lua:class:`settings.settings` object. If the corresponding
    JSON file does not exist it will be created.

    :param name: The name of the settings object and corresponding file.
    :type name: string
    :return: A new settings object
    :rtype: settings.settings

    .. note::
        While not strictly required, module authors are encouraged to name
        settings after the module that creates them, ie `console.lua`.

    .. versionhistory::
        :0.0.1: Added
*/
int settings_lua_new(lua_State *L) {
    const char *name = luaL_checkstring(L, 1);

    settings_t *settings = settings_new(name);

    lua_pushsettings(L, settings);
    settings_unref(settings);
    
    return 1;
}

/*** RST
Classes
-------

.. lua:class:: settings

    A settings object.
*/

luaL_Reg settings_lua_funcs[] = {
    "__gc"      , &settings_lua_del,
    "get"       , &settings_lua_get,
    "set"       , &settings_lua_set,
    "setdefault", &settings_lua_set_default,
    "saveonset" , &settings_lua_save_on_set,
    "save"      , &settings_lua_save,
    NULL        ,  NULL
};

void settings_lua_register_metatable(lua_State *L) {
    if (luaL_newmetatable(L, "SettingsMetaTable")) {
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");

        luaL_setfuncs(L, settings_lua_funcs, 0);
    }
}

void lua_pushsettings(lua_State *L, settings_t *settings) {
    settings_t **psettings = lua_newuserdata(L, sizeof(settings_t*));
    *psettings = settings;

    settings_lua_register_metatable(L);
    lua_setmetatable(L, -2);
    settings_ref(settings);
}

int settings_lua_del(lua_State *L) {
    settings_t *s = lua_checksettings(L, 1);

    settings_unref(s);

    return 0;
}

/*** RST
    .. lua:method:: set(key, value)

        Set the value for the given key. If the key already exists it will be overridden.

        :param key: The key to set.
        :type key: string
        :param value: The value to set. Tables are not supported, objects or
            arrays should be passed as :lua:class:`jansson.json` objects instead.
        :type value: number, boolean, nil, string, or :lua:class:`jansson.json`

        .. versionhistory::
            :0.0.1: Added
*/
int settings_lua_set(lua_State *L) {
    settings_t *s = lua_checksettings(L, 1);
    const char *key = luaL_checkstring(L, 2);

    switch(lua_type(L, 3)) {
    case LUA_TUSERDATA: { 
        json_t *val = lua_checkjson(L, 3);
        settings_set(s, key, val);
        json_decref(val);
        break; }
    case LUA_TNUMBER: {
        if (lua_isinteger(L, 3)) {
            int val = (int)lua_tointeger(L, 3);
            settings_set_int(s, key, val);
        } else {
            double val = lua_tonumber(L, 3);
            settings_set_double(s, key, val);
        }
        break; }
    case LUA_TBOOLEAN: {
        int val = lua_toboolean(L, 3);
        settings_set_boolean(s, key, val);
        break; }
    case LUA_TNIL:
        logger_warn(s->log, "ignoring nil value for %s", key);
        break;
    case LUA_TSTRING: {
        const char *val = lua_tostring(L, 3);
        settings_set_string(s, key, val);
        break;}
    default:
        logger_error(s->log, "Don't know how to handle lua data type.");
        break;
    }

    return 0;
}

/*** RST
    .. lua:method:: setdefault(key, value)

        Set a default value for a given key. This functions exactly like
        :lua:meth:`settings.set` except this does not write the value to the
        underlying JSON file.

        :param key: The key to set.
        :type key: string
        :param value: The value to set. Tables are not supported, objects or
            arrays should be passed as :lua:class:`jansson.json` objects instead.
        :type value: number, boolean, nil, string, or :lua:class:`jansson.json`

        .. versionhistory::
            :0.0.1: Added
*/
int settings_lua_set_default(lua_State *L) {
    settings_t *s = lua_checksettings(L, 1);
    const char *key = luaL_checkstring(L, 2);

    switch(lua_type(L, 3)) {
    /*
    case LUA_TUSERDATA: { 
        json_t *val = lua_checkjson(L, 3);
        settings_set_default(s, key, val);
        json_decref(val);
        break; }
    */
    case LUA_TNUMBER: {
        if (lua_isinteger(L, 3)) {
            int val = (int)lua_tointeger(L, 3);
            settings_set_default_int(s, key, val);
        } else {
            double val = lua_tonumber(L, 3);
            settings_set_default_double(s, key, val);
        }
        break; }
    case LUA_TBOOLEAN: {
        int val = lua_toboolean(L, 3);
        settings_set_default_boolean(s, key, val);
        break; }
    case LUA_TNIL:
        logger_warn(s->log, "ignoring nil value for %s", key);
        break;
    case LUA_TSTRING: {
        const char *val = lua_tostring(L, 3);
        settings_set_default_string(s, key, val);
        break;}
    default:
        return luaL_error(L, "Value can't be used as settings default.");
        break;
    }

    return 0;
}

/*** RST
    .. lua:method:: get(key)

        Get the value for the given key. If the key doesn't exist, return the
        default value, if set. If not, returns ``nil``.

        Object and arrays are returned as :lua:class:`jansson.json` objects.

        :param key: The key to return. This is a path into the JSON object
            structure separated by ``.``. I.e. ``window.x``.
        :type key: string
        :return: The config value at ``key`` or ``nil``

        .. versionhistory::
            :0.0.1: Added
*/
int settings_lua_get(lua_State *L) {
    settings_t *s = lua_checksettings(L, 1);
    const char *key = luaL_checkstring(L, 2);

    json_t *val = NULL;
    settings_get_internal(s, key, &val);

    if (!val) {
        uint32_t key_ind = settings_get_default_key_ind(s, key);
        if (s->default_keys[key_ind] && strcmp(s->default_keys[key_ind], key)==0) {
            settings_default_t *dv = s->default_values[key_ind];
            switch (dv->type) {
            case SETTINGS_DEFAULT_TYPE_BOOLEAN:
                lua_pushboolean(L, dv->value_int);
                break;
            case SETTINGS_DEFAULT_TYPE_DOUBLE:
                lua_pushnumber(L, dv->value_double);
                break;
            case SETTINGS_DEFAULT_TYPE_INT:
                lua_pushnumber(L, dv->value_int);
                break;
            case SETTINGS_DEFAULT_TYPE_JSON:
                lua_pushjson(L, dv->value_json);
                break;
            case SETTINGS_DEFAULT_TYPE_STRING:
                lua_pushstring(L, dv->value_str);
                break;
            }
            return 1;
        }

        //logger_warn(s->log, "[%s] UNKNOWN key: %s", s->file_path, key);
        return 0;
    }
    

    switch (json_typeof(val)) {
    case JSON_ARRAY:
    case JSON_OBJECT:
        lua_pushjson(L, val);
        break;
    case JSON_TRUE:
    case JSON_FALSE: 
        lua_pushboolean(L, json_boolean_value(val));
        break;
    case JSON_REAL: 
        lua_pushnumber(L, json_real_value(val));
        break;
    case JSON_INTEGER:
        lua_pushinteger(L, json_integer_value(val));
        break;
    case JSON_STRING:
        lua_pushstring(L, json_string_value(val));
        break;
    default:
        logger_warn(s->log, "[%s] GET %s can't handle JSON type.", s->file_path, key);
        return 0;
    }

    return 1;
}

settings_t *lua_checksettings(lua_State *L, int ind) {
    return *(settings_t**)luaL_checkudata(L, ind, "SettingsMetaTable");
}

/*** RST
    .. lua:method:: saveonset(value)

        Control the behvior when setting a value. By default,
        :lua:class:`settings.settings` will save values to the underlying JSON
        file every time a value is set. This behavior can be turned off by
        passing ``false`` to this function.

        :param value: Save on set?
        :type key: boolean

        .. versionhistory::
            :0.0.1: Added
*/
int settings_lua_save_on_set(lua_State *L) {
    settings_t *settings = lua_checksettings(L, 1);

    settings->save_on_set = (uint8_t)lua_toboolean(L, 2);

    return 0;
}

/*** RST
    .. lua:method:: save()

        Save the :lua:class:`settings.settings` object to the underlying JSON
        file. This is required if :lua:meth:`saveonset` is turned off.

        .. versionhistory::
            :0.0.1: Added
*/
int settings_lua_save(lua_State *L) {
    settings_t *settings = lua_checksettings(L, 1);

    settings_save(settings);

    return 0;
}
