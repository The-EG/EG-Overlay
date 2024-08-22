#include <windows.h>
#include <ShlObj.h>
#include <psapi.h>
#include <glad/gl.h>
#include <glfw/glfw3.h>
#include "app.h"
#include "lua-manager.h"
#include "logging/helpers.h"
#include "utils.h"
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <string.h>
#include "lua-json.h"

typedef struct lua_event_callback_list_t {
    lua_manager_event_callback *cb;
    void *data;
    struct lua_event_callback_list_t *next;
} lua_event_callback_list_t;

typedef struct lua_event_list_t {
    char *event;
    json_t *data;
    int data_cbi;

    struct lua_event_list_t *next;
} lua_event_list_t;

typedef struct lua_event_handler_t {
    int cbi;
    struct lua_event_handler_t *next;
} lua_event_handler_t;

typedef struct lua_manager_module_opener_t {
    char *name;
    lua_manager_module_opener_fn *opener_fn;

    struct lua_manager_module_opener_t *next;
} lua_manager_module_opener_t;

typedef struct lua_manager_coroutine_thread_list_t {
    lua_State *thread;
    int threadi;
    struct lua_manager_coroutine_thread_list_t *next;
} lua_manager_coroutine_thread_list_t;

typedef struct {
    logger_t *log;
    lua_State *lua;

    lua_event_callback_list_t *event_cbs;

    // event handler hash table.
    // each event type (string) points to a linked list of event handlers
    size_t event_handler_table_size;
    char **event_handler_types;
    lua_event_handler_t **event_handlers;

    lua_manager_module_opener_t *module_openers;
    lua_manager_coroutine_thread_list_t *coroutine_threads;
} lua_manager_t;

// stored statically so that events can be queued before Lua is even setup
static lua_event_list_t *event_queue = NULL;
static int no_events = 0;

static lua_manager_t *lua = NULL;

char *get_lua_module_path(lua_State *L, int stack_depth);

static int overlay_add_event_handler(lua_State *L);
static int overlay_remove_event_handler(lua_State *L);
static int overlay_queue_event(lua_State *L);
static int overlay_log(lua_State *L);
static int overlay_time(lua_State *L);
static int overlay_settings(lua_State *L);
static int overlay_mem_usage(lua_State *L);
static int overlay_process_time(lua_State *L);
static int overlay_data_folder(lua_State *L);
static int overlay_clipboard_text(lua_State *L);
static int overlay_exit(lua_State *L);

static luaL_Reg overlay_funcs[] = {
    "add_event_handler"   , &overlay_add_event_handler,
    "remove_event_handler", &overlay_remove_event_handler,
    "queue_event"         , &overlay_queue_event,
    "log"                 , &overlay_log,
    "time"                , &overlay_time,
    "settings"            , &overlay_settings,
    "mem_usage"           , &overlay_mem_usage,
    "process_time"        , &overlay_process_time,
    "data_folder"         , &overlay_data_folder,
    "clipboard_text"      , &overlay_clipboard_text,
    "exit"                , &overlay_exit,
    NULL                  ,  NULL
};

static int open_overlay_module(lua_State *L) {
    lua_newtable(L);
    luaL_setfuncs(L, overlay_funcs, 0);

    return 1;
}

static int embedded_module_searcher(lua_State *L) {
    const char *name = luaL_checkstring(L, 1);

    lua_manager_module_opener_t *mo = lua->module_openers;
    while (mo) {
        if (strcmp(mo->name, name)==0) {
            lua_pushcfunction(L, mo->opener_fn);
            lua_pushliteral(L, ":eg-overlay-embedded-module:");
            return 2;
        }
        mo = mo->next;
    }

    lua_pushnil(L);

    return 1;
}

void lua_manager_init() {
    lua = calloc(1, sizeof(lua_manager_t));

    lua->log = logger_get("lua");

    logger_info(lua->log, "Initializing Lua...");

    lua->event_handler_table_size = 512;
    lua->event_handler_types = calloc(lua->event_handler_table_size, sizeof(char*));
    lua->event_handlers = calloc(lua->event_handler_table_size, sizeof(lua_event_handler_t*));

    lua->lua = luaL_newstate();

    if (!lua->lua) {
        logger_error(lua->log, "Couldn't initialize new Lua state.");
        error_and_exit("EG-Overlay", "Couldn't initialize new Lua state.");
    }

    luaL_openlibs(lua->lua);

    // add our embedded module searcher
    lua_getglobal(lua->lua, "package");
    lua_getfield(lua->lua, -1, "searchers");
    lua_pushcfunction(lua->lua, &embedded_module_searcher);
    lua_seti(lua->lua, -2, luaL_len(lua->lua, -2) + 1);
    lua_pop(lua->lua, 2);

    lua_manager_add_module_opener("eg-overlay", &open_overlay_module);
}

void lua_manager_cleanup() {
    // don't queue events from now on
    no_events = 1;

    for (size_t e=0;e<lua->event_handler_table_size;e++) {
        if (lua->event_handler_types[e]) {
            free(lua->event_handler_types[e]);
            lua_event_handler_t *h = lua->event_handlers[e];
            while (h) {
                lua_event_handler_t *next = h->next;
                luaL_unref(lua->lua, LUA_REGISTRYINDEX, h->cbi);
                free(h);
                h = next;
            }
        }
    }
    free(lua->event_handlers);
    free(lua->event_handler_types);

    lua_manager_module_opener_t *mo = lua->module_openers;
    while (mo) {
        free(mo->name);
        lua_manager_module_opener_t *next = mo->next;
        free(mo);
        mo = next;
    }    

    lua_close(lua->lua);
}

void lua_manager_add_module_opener(const char *name, lua_manager_module_opener_fn *opener_fn) {
    lua_manager_module_opener_t *mo = calloc(1, sizeof(lua_manager_module_opener_t));
    mo->opener_fn = opener_fn;
    mo->name = calloc(strlen(name) + 1, sizeof(char));
    memcpy(mo->name, name, strlen(name));

    if (lua->module_openers==NULL) {
        lua->module_openers = mo;
        return;
    }

    lua_manager_module_opener_t *last_mo = lua->module_openers;
    while (last_mo->next) last_mo = last_mo->next;

    last_mo->next = mo;
}

void lua_manager_run_file(const char *path) {

    lua_State *thread = lua_newthread(lua->lua);

    int r = luaL_loadfile(thread, path);

    if (r != LUA_OK) {
        const char *err_msg = lua_tolstring(thread, -1, NULL);
        logger_error(lua->log, "Couldn't load %s: %s", path, err_msg);
        error_and_exit("EG-Overlay: Lua", "Couldn't load %s:\n%s", path, err_msg);
    }

    int nres = 0;

    while ((r=lua_resume(thread, NULL, 0, &nres))==LUA_YIELD) {
        lua_manager_run_events();
        while(lua_manager_resume_coroutines()) {}
        lua_manager_run_event_queue();        
    }

    if (r!=LUA_OK) {
        // error occurred
        const char *errmsg = luaL_checkstring(thread, -1);
        logger_error(lua->log, "Error occurred during lua run file: %s", errmsg);
        lua_pop(thread, 1);
        // pop the thread
        lua_closethread(thread, NULL);
    }
}

char *lua_manager_get_lua_module_name2(lua_State *L, int stack_depth) {
    char *path = get_lua_module_path(L, stack_depth);
    char *name = NULL;

    size_t start = 0;

    for(size_t i=strlen(path)-1;i>0;i--) {
        if (path[i]=='\\' || path[i]=='/') {
            start = i + 1;
            break;
        }
    }

    size_t name_len = strlen(path) - start;
    name = calloc(name_len+1, sizeof(char));
    memcpy(name, path + start, name_len);

    free(path);

    return name;
}

char *lua_manager_get_lua_module_name(lua_State *L) {
    return lua_manager_get_lua_module_name2(L, 1);
}

char *lua_manager_get_lua_module_name_and_line2(lua_State *L, int stack_depth) {
    char *name = lua_manager_get_lua_module_name2(L, stack_depth);
    lua_Debug st;
    lua_getstack(L, stack_depth, &st);
    lua_getinfo(L, "l", &st);

    char *nal = calloc(strlen(name) + 1 + 7, sizeof(char));
    memcpy(nal, name, strlen(name));
    nal[strlen(name)] = ':';
    snprintf(nal + strlen(name) + 1, 6, "%d", st.currentline);
    free(name);

    return nal;
}

char *lua_manager_get_lua_module_name_and_line(lua_State *L) {
    return lua_manager_get_lua_module_name_and_line2(L, 1);
}

char *get_lua_module_path(lua_State *L, int stack_depth) {
    lua_Debug st;
    lua_getstack(L, stack_depth, &st);
    lua_getinfo(L, "S", &st);

    char *path = calloc(strlen(st.source)+1, sizeof(char));
    memcpy(path, st.source, strlen(st.source));

    return path;
}

/*** RST
eg-overlay
===========

.. lua:module:: eg-overlay

.. code:: lua

    local overlay = require 'eg-overlay'

The :lua:mod:`eg-overlay` module contains core functions that other modules use to communicate with the overlay.

Functions
---------
*/

/*** RST
.. lua:function:: add_event_handler(event, handler)

    Add an event handler for the given event name.
    
    The handler function will be called every time that particular event is posted with two arguments: the event name and event data. The may be ``nil``, any Lua data type or a :lua:class:`JSON.json` object.

    :param event: Event type
    :type event: string
    :param handler: Function to be called on the given event
    :type handler: function
    :return: A callback ID that can be used with :lua:func:`remove_event_handler`.
    :rtype: integer

    .. versionhistory::
        :0.0.1: Added
*/
static int overlay_add_event_handler(lua_State *L) {
    const char *event = luaL_checkstring(L, 1);

    if (lua_type(L, 2)!=LUA_TFUNCTION) {
        return luaL_error(L, "overlay.add_event_handler: argument #2 must be a function.");
    }
    
    lua_pushvalue(L, 2);
    int cbi = luaL_ref(L, LUA_REGISTRYINDEX);

    lua_event_handler_t *h = calloc(1, sizeof(lua_event_handler_t));
    h->cbi = cbi;
    
    uint32_t event_hash = djb2_hash_string(event);
    uint32_t event_ind = event_hash % lua->event_handler_table_size;

    while (lua->event_handler_types[event_ind]!=NULL && strcmp(lua->event_handler_types[event_ind], event)!=0) {
        // key hash collision, linear probe
        event_ind++;
        if (event_ind >= lua->event_handler_table_size) event_ind = 0;
        if (event_ind == event_hash % lua->event_handler_table_size) {
            return luaL_error(L, "Event handler hash table full.");
        }
    }

    if (lua->event_handler_types[event_ind]==NULL) {
        lua->event_handler_types[event_ind] = calloc(strlen(event) + 1, sizeof(char));
        memcpy(lua->event_handler_types[event_ind], event, strlen(event));
    }

    if (lua->event_handlers[event_ind]==NULL) {
        lua->event_handlers[event_ind] = h;
    } else {
        lua_event_handler_t *eh = lua->event_handlers[event_ind];
        while (eh->next) eh = eh->next;
        eh->next = h;
    }

    char *mod = lua_manager_get_lua_module_name(L);
    logger_info(lua->log, "%s registered for %s events.", mod, event);
    free(mod);

    lua_pushinteger(L, cbi);

    return 1;
}

/*** RST
.. lua:function:: remove_event_handler(event, cbi)

    Remove an event handler for the given event name. The callback ID is returned by :lua:func:`add_event_handler`.

    :param event: Event type
    :type event: string
    :param cbi: Callback ID
    :type cbi: integer
    :return: none

    .. versionhistory::
        :0.0.1: Added
*/
static int overlay_remove_event_handler(lua_State *L) {
    const char *event = luaL_checkstring(L, 1);
    int cbi = (int)luaL_checkinteger(L, 2);

    uint32_t event_hash = djb2_hash_string(event);
    uint32_t event_ind = event_hash % lua->event_handler_table_size;

    while (lua->event_handler_types[event_ind]!=NULL && strcmp(lua->event_handler_types[event_ind], event)!=0) {
        // key hash collision, linear probe
        event_ind++;
        if (event_ind >= lua->event_handler_table_size) event_ind = 0;
        if (event_ind == event_hash % lua->event_handler_table_size) {
            return luaL_error(L, "Event handler hash table full.");
        }
    }

    if (lua->event_handler_types[event_ind]==NULL || strcmp(lua->event_handler_types[event_ind], event)!=0) {
        return luaL_error(L, "Event type not found.");
    }

    lua_event_handler_t *h = lua->event_handlers[event_ind];
    lua_event_handler_t *prev = NULL;
    while (h->next && h->cbi!=cbi) { 
        prev = h; 
        h = h->next;
    }

    if (h->cbi!=cbi) return 0;

    luaL_unref(L, LUA_REGISTRYINDEX, h->cbi);

    if (prev) prev->next = h->next;
    if (prev==NULL) lua->event_handlers[event_ind] = h->next;
    free(h);

    char *mod = lua_manager_get_lua_module_name(L);
    logger_info(lua->log, "%s unregistered for %s events.", mod, event);
    free(mod);

    return 0;
}

/*** RST
.. lua:function:: queue_event(event, data)

    Add a new event to the queue.

    :param event: Event type
    :type event: string
    :param data: Optional event data
    :return: none

    .. note::
        Events are dispatched in the order they are queued each render frame. Events that are queued during an event callback will be dispatched on the following frame.

    .. versionhistory::
        :0.0.1: Added
*/
static int overlay_queue_event(lua_State *L) {
    const char *event = luaL_checkstring(L, 1);
    int data_cbi = 0;

    if (lua_gettop(L)==2) {
        lua_pushvalue(L, 2);
        data_cbi = luaL_ref(L, LUA_REGISTRYINDEX);
    }

    lua_event_list_t *e = calloc(1, sizeof(lua_event_list_t));
    e->data_cbi = data_cbi;
    e->event = calloc(strlen(event)+1, sizeof(char));
    memcpy(e->event, event, strlen(event));

    if (event_queue==NULL) {
        event_queue = e;
        return 0;
    }

    lua_event_list_t *eq = event_queue;
    while (eq->next) eq = eq->next;

    eq->next = e;

    return 0;
}

static int overlay_log(lua_State *L) {
    const char *name = luaL_checkstring(L, 1);
    enum LOGGER_LEVEL level = (enum LOGGER_LEVEL)luaL_checkinteger(L, 2);
    const char *message = luaL_checkstring(L, 3);

    logger_t *log = logger_get(name);
    logger_log(log, level, "%s", message);

    return 0;
}

/*** RST
.. lua:function:: time()

    Returns the fractional number of seconds since the overlay was started. This can be used to time animations, events, etc.

    :return: Time value
    :rtype: number

    .. versionhistory::
        :0.0.1: Added
*/
static int overlay_time(lua_State *L) {
    lua_pushnumber(L, glfwGetTime());

    return 1;
}

/*** RST
.. lua:function:: settings()

    Returns a :lua:class:`settings` object for the overlay itself, which holds core settings and defaults.

    :rtype: settings

    .. versionhistory::
        :0.0.1: Added
*/
static int overlay_settings(lua_State *L) {
    lua_pushsettings(L, app_get_settings());
        
    return 1;
}

void lua_manager_unref(int cbi) {
    luaL_unref(lua->lua, LUA_REGISTRYINDEX, cbi);
}

int lua_manager_gettableref_bool(int table_ind, const char *field) {
    lua_rawgeti(lua->lua, LUA_REGISTRYINDEX, table_ind);

    lua_getfield(lua->lua, -1, field);

    int ret = lua_toboolean(lua->lua, -1);
    lua_pop(lua->lua, 2);

    return ret;
}

void lua_manager_settabletref_bool(int table_ind, const char *field, int value) {
    lua_rawgeti(lua->lua, LUA_REGISTRYINDEX, table_ind);
    lua_pushboolean(lua->lua, value);
    lua_setfield(lua->lua, -2, field);
    lua_pop(lua->lua, 1);
}

void lua_manager_add_event_callback(lua_manager_event_callback *cb, void *data) {
    lua_event_callback_list_t *e = calloc(1, sizeof(lua_event_callback_list_t));
    e->cb = cb;
    e->data = data;

    if (lua->event_cbs==NULL) {
        lua->event_cbs = e;
        return;
    }

    lua_event_callback_list_t *l = lua->event_cbs;
    while (l->next) l = l->next;

    l->next = e;
}

static lua_event_handler_t *get_event_handlers(const char *event) {
    uint32_t event_hash = djb2_hash_string(event);
    uint32_t event_ind = event_hash % lua->event_handler_table_size;

    while (lua->event_handler_types[event_ind]!=NULL && strcmp(lua->event_handler_types[event_ind], event)!=0) {
        // key hash collision, linear probe
        event_ind++;
        if (event_ind >= lua->event_handler_table_size) event_ind = 0;
        if (event_ind == event_hash % lua->event_handler_table_size) {
            return NULL;
        }
    }

    if (lua->event_handler_types[event_ind]==NULL || strcmp(lua->event_handler_types[event_ind], event)!=0) {
        return NULL;
    }

    return lua->event_handlers[event_ind];
}

void lua_manager_add_coroutine_thread(lua_State *thread, int threadi) {
    lua_manager_coroutine_thread_list_t *coroutine = calloc(1, sizeof(lua_manager_coroutine_thread_list_t));
    coroutine->thread = thread;
    coroutine->threadi = threadi;

    if (lua->coroutine_threads==NULL) {
        lua->coroutine_threads = coroutine;
        return;
    }
    
    lua_manager_coroutine_thread_list_t *c = lua->coroutine_threads;
    while (c->next) c = c->next;
    c->next = coroutine;
}

void lua_manager_remove_coroutine_thread(int threadi) {
    lua_manager_coroutine_thread_list_t *c = lua->coroutine_threads;
    lua_manager_coroutine_thread_list_t *p = NULL;

    while (c) {
        if (c->threadi == threadi) {
            luaL_unref(lua->lua, LUA_REGISTRYINDEX, c->threadi);
            lua_closethread(c->thread, NULL);
            if (p) p->next = c->next;
            if (lua->coroutine_threads==c) lua->coroutine_threads = c->next;
            free(c);
            return;
        }
        p = c;
        c = c->next;
    }
}

void lua_manager_run_events() {
    lua_event_callback_list_t *e = lua->event_cbs;
    lua_event_callback_list_t *prev = NULL;
    while (e) {
        lua_event_callback_list_t *next = e->next;

        lua_State *cothread = lua_newthread(lua->lua);

        int narg = e->cb(cothread, e->data);

        
        int nres = 0;
        int status = lua_resume(cothread, NULL, narg, &nres);

        if (status==LUA_YIELD) {
            // the event handler yielded, save the thread and resume it later
            if (nres) lua_pop(cothread, nres);
            
            // store the thread so it doesn't get GCd
            int threadi = luaL_ref(lua->lua, LUA_REGISTRYINDEX);
            lua_manager_add_coroutine_thread(cothread, threadi);
        } else if (status==LUA_OK) {
            // no coroutine so just close the thread
            if (nres) lua_pop(cothread, nres);
            // pop the thread
            lua_pop(lua->lua, 1);
            lua_closethread(cothread, NULL);            
        } else {
            // error occurred
            const char *errmsg = luaL_checkstring(cothread, -1);
            logger_error(lua->log, "Error occurred during lua callback event handler: %s", errmsg);
            lua_pop(cothread, 1);
            // pop the thread
            lua_pop(lua->lua, 1);
            lua_closethread(cothread, NULL);
        }
        
        if (lua->event_cbs==e) lua->event_cbs = e->next;
        free(e);
        if (prev) prev->next = e->next;
        e = next;
    }
}

int lua_manager_resume_coroutines() {
    lua_manager_coroutine_thread_list_t *c = lua->coroutine_threads;
    lua_manager_coroutine_thread_list_t *p = NULL;

    while (c) {
        int nres = 0;
        int status = lua_resume(c->thread, NULL, 0, &nres);

        if (status==LUA_YIELD) {
            // coroutine yielded again, leave it in the list
            if (nres) lua_pop(c->thread, nres);
            p = c;
            c = c->next;
        } else if (status==LUA_OK) {
            // coroutine finished, remove it and free thread
            luaL_unref(lua->lua, LUA_REGISTRYINDEX, c->threadi);
            lua_closethread(c->thread, NULL);
            if (p) p->next = c->next;
            if (lua->coroutine_threads==c) lua->coroutine_threads = c->next;
            lua_manager_coroutine_thread_list_t *n = c->next;
            free(c);
            c = n;
        } else {
            // error, clean up the thread
            const char *errmsg = luaL_checkstring(c->thread, -1);
            logger_error(lua->log, "Error while resuming event coroutine: %s", errmsg);
            lua_pop(c->thread, 1);
            luaL_unref(lua->lua, LUA_REGISTRYINDEX, c->threadi);
            lua_closethread(c->thread, NULL);
            if (p) p->next = c->next;
            if (lua->coroutine_threads==c) lua->coroutine_threads = c->next;
            lua_manager_coroutine_thread_list_t *n = c->next;
            free(c);
            c = n;
        }
    }
    return lua->coroutine_threads ? 1 : 0;
}

void lua_manager_call_event_handlers(const char *event, int data_cbi) {
    lua_event_handler_t *h = get_event_handlers(event);
    lua_event_handler_t *next;
    while (h) {
        // get the reference to the next handler since this one might remove itself
        next = h->next;

        // run each event in its own thread, this way event handlers can
        // be coroutines and yield
        lua_State *cothread = lua_newthread(lua->lua);

        lua_rawgeti(cothread, LUA_REGISTRYINDEX, h->cbi);
        lua_pushstring(cothread, event);
        if (data_cbi) {
            lua_rawgeti(cothread, LUA_REGISTRYINDEX, data_cbi);
        } else lua_pushnil(cothread);

        int nres = 0;
        int status = lua_resume(cothread, NULL, 2, &nres);

        if (status==LUA_YIELD) {
            // the event handler yielded, save the thread and resume it later
            if (nres) lua_pop(cothread, nres);
            
            // store the thread so it doesn't get GCd
            int threadi = luaL_ref(lua->lua, LUA_REGISTRYINDEX);
            lua_manager_add_coroutine_thread(cothread, threadi);
        } else if (status==LUA_OK) {
            // no coroutine so just close the thread
            if (nres) lua_pop(cothread, nres);
            // pop the thread
            lua_pop(lua->lua, 1);
            lua_closethread(cothread, NULL);            
        } else {
            // error occurred
            const char *errmsg = luaL_checkstring(cothread, -1);
            logger_error(lua->log, "Error occurred during lua event handler (%s): %s", event, errmsg);
            lua_pop(cothread, 1);
            // pop the thread
            lua_pop(lua->lua, 1);
            lua_closethread(cothread, NULL);
        }
        h = next;
    }
}

void lua_manager_run_event_queue() {
    lua_event_list_t *eq = event_queue;
    
    // clear the event queue now. any events added during these events will be
    // added to a 'new' queue that will be dispatched next frame
    event_queue = NULL;

    while (eq) {
        lua_event_list_t *next = eq->next;

        if (eq->data && !eq->data_cbi) {
            lua_pushjson(lua->lua, eq->data);
            json_decref(eq->data);
            eq->data_cbi = luaL_ref(lua->lua, LUA_REGISTRYINDEX);
        }

        lua_manager_call_event_handlers(eq->event, eq->data_cbi);

        free(eq->event);
        if (eq->data_cbi) luaL_unref(lua->lua, LUA_REGISTRYINDEX, eq->data_cbi);
        free(eq);
        eq = next;
    }
}

void lua_manager_queue_event(const char *event, json_t *data) {
    //if (!lua || !lua->lua) return; // lua not yet initialized
    if (no_events) {
        if (data) json_decref(data);   
        return;
    }

    lua_event_list_t *e = calloc(1, sizeof(lua_event_list_t));
    if (data) {
        e->data = data;
        json_incref(data);
    }
    e->event = calloc(strlen(event)+1, sizeof(char));
    memcpy(e->event, event, strlen(event));

    if (event_queue==NULL) {
        event_queue = e;
        return;
    }

    lua_event_list_t *eq = event_queue;
    while (eq->next) eq = eq->next;

    eq->next = e;
}

/*** RST
.. lua:function:: mem_usage()

    Returns a table containing the memory usage of the overlay. These statistics are for the application as a whole, and not just Lua.

    :return: Memory usage
    :rtype: table

    .. luatablefields::
        :working_set: The current memory in use by the overlay, in bytes.
        :peak_working_set: The maximum amount of memory the overlay has used since starting, in bytes.

    .. versionhistory::
        :0.0.1: Added
*/
static int overlay_mem_usage(lua_State *L) {
    PROCESS_MEMORY_COUNTERS mem_counters = {0};
    mem_counters.cb = sizeof(PROCESS_MEMORY_COUNTERS);

    if (!GetProcessMemoryInfo(GetCurrentProcess(), &mem_counters, mem_counters.cb)) {
        logger_warn(lua->log, "Couldn't get process memory counters.");
        return 0;
    }

    lua_newtable(L);
    lua_pushinteger(L, mem_counters.WorkingSetSize);
    lua_setfield(L, -2, "working_set");
    lua_pushinteger(L, mem_counters.PeakWorkingSetSize);
    lua_setfield(L, -2, "peak_working_set");

    return 1;
}

/*** RST
.. lua:function:: process_time()

    Returns a table containing the CPU time and uptime of the overlay. This can be used to calculate the overlay's CPU usage.

    :return: Process time
    :rtype: table

    .. luatablefields::
        :process_time_total: The total time the overlay has been running.
        :user_time: The total time spent executing code within the overlay.
        :kernel_time: The total time spent executing system code for the overlay, not including idle time.
        :system_user_time: The total time spent executing all application code on the system.
        :system_kernel_time: The total time spent executing system code on the system, including idle time.

    .. versionhistory::
        :0.0.1: Added
*/
static int overlay_process_time(lua_State *L) {
    FILETIME create_time = {0};
    FILETIME exit_time = {0};
    FILETIME kernel_time = {0};
    FILETIME user_time = {0};

    FILETIME sys_idle;
    FILETIME sys_user;
    FILETIME sys_kernel;

    GetProcessTimes(GetCurrentProcess(), &create_time, &exit_time, &kernel_time, &user_time);
    GetSystemTimes(&sys_idle, &sys_kernel, &sys_user);

    FILETIME ft_now;
    SYSTEMTIME st_now;
    GetSystemTime(&st_now);
    SystemTimeToFileTime(&st_now, &ft_now);

    ULARGE_INTEGER uli_create = {0};
    uli_create.HighPart = create_time.dwHighDateTime;
    uli_create.LowPart = create_time.dwLowDateTime;

    ULARGE_INTEGER uli_now = {0};
    uli_now.HighPart = ft_now.dwHighDateTime;
    uli_now.LowPart = ft_now.dwLowDateTime;

    double total_time_milliseconds = (uli_now.QuadPart - uli_create.QuadPart) / 10000.0;

    ULARGE_INTEGER uli_user = {0};
    uli_user.HighPart = user_time.dwHighDateTime;
    uli_user.LowPart = user_time.dwLowDateTime;

    double user_milliseconds = uli_user.QuadPart / 10000.0;

    ULARGE_INTEGER uli_kernel = {0};
    uli_kernel.HighPart = kernel_time.dwHighDateTime;
    uli_kernel.LowPart = kernel_time.dwLowDateTime;

    double kernel_milliseconds = uli_kernel.QuadPart / 10000.0;

    // ULARGE_INTEGER uli_sys_idle;
    // uli_sys_idle.HighPart = sys_idle.dwHighDateTime;
    // uli_sys_idle.LowPart = sys_idle.dwLowDateTime;

    ULARGE_INTEGER uli_sys_user = {0};
    uli_sys_user.HighPart = sys_user.dwHighDateTime;
    uli_sys_user.LowPart = sys_user.dwLowDateTime;

    double sys_user_milliseconds = uli_sys_user.QuadPart / 10000.0;

    ULARGE_INTEGER uli_sys_kernel = {0};
    uli_sys_kernel.HighPart = sys_kernel.dwHighDateTime;
    uli_sys_kernel.LowPart = sys_kernel.dwLowDateTime;

    double sys_kernel_milliseconds = uli_sys_kernel.QuadPart / 10000.0;

    lua_newtable(L);
    lua_pushnumber(L, total_time_milliseconds);
    lua_setfield(L, -2, "process_time_total");

    lua_pushnumber(L, user_milliseconds);
    lua_setfield(L, -2, "user_time");

    lua_pushnumber(L, kernel_milliseconds);
    lua_setfield(L, -2, "kernel_time");

    lua_pushnumber(L, sys_user_milliseconds);
    lua_setfield(L, -2, "system_user_time");

    lua_pushnumber(L, sys_kernel_milliseconds);
    lua_setfield(L, -2, "system_kernel_time");

    return 1;
}

/*** RST
.. lua:function:: overlay_data_folder(name)

    Returns the full path to the data folder for the given module. 
    
    Modules should store any data other than settings in this folder. The folder will be created by this function if it does not already exist.

    :param name: The name of the module and corresponding folder.
    :type name: string
    :return: The full path to the module data folder.
    :rtype: string

    .. versionhistory::
        :0.0.1: Added
*/
static int overlay_data_folder(lua_State *L) {
    const char *name = luaL_checkstring(L, 1);

    size_t proc_path_len = MAX_PATH;
    char *proc_path = calloc(proc_path_len+1, sizeof(char));

    proc_path_len = GetModuleFileName(NULL, proc_path, (DWORD)proc_path_len);
    if (!proc_path_len) {
        return luaL_error(L, "Couldn't get module filename.");
    }

    size_t last_sep = 0;
    for (size_t i=proc_path_len - 1;i >0;i--) {
        if (proc_path[i]=='\\') {
            last_sep = i;
            break;
        }
    }

    size_t data_path_len = last_sep + strlen("\\data\\") + strlen(name) + 2;
    char *data_path = calloc(data_path_len, sizeof(char));

    memcpy(data_path, proc_path, last_sep);
    memcpy(data_path + last_sep, "\\data\\",strlen("\\data\\"));
    memcpy(data_path + last_sep + strlen("\\data\\"), name, strlen(name));
    data_path[data_path_len-2] = '\\';

    switch(SHCreateDirectoryEx(NULL, data_path, NULL)) {
    case ERROR_BAD_PATHNAME:
        free(data_path);
        return luaL_error(L, "Couldn't create data directory: bad pathname.");
    case ERROR_FILENAME_EXCED_RANGE:
        free(data_path);
        return luaL_error(L, "Couldn't create data directory: filename exceeded range.");
    case ERROR_PATH_NOT_FOUND:
        free(data_path);
        return luaL_error(L, "Couldn't create data directory: path not found.");
    case ERROR_CANCELLED:
        free(data_path);
        return luaL_error(L, "Couldn't create data directory: cancelled.");
    }

    lua_pushstring(L, data_path);
    free(data_path);

    return 1;
}

/*** RST
.. lua:function:: clipboard_text([text])

    Set or return the text on the clipboard.

    :param text: (Optional) If present, the text to set the clipboard to.
    :type text: string
    :return: If ``text`` is ``nil``, returns the text currently on the clipboard. Otherwise ``nil``.
    :rtype: string

    .. versionhistory::
        :0.0.1: Added
*/
static int overlay_clipboard_text(lua_State *L) {    
    if (lua_gettop(L)==1) {
        const char *text = luaL_checkstring(L, 1);
        app_setclipboard_text(text);
        return 0;
    } else if (lua_gettop(L)==0) {
        return luaL_error(L, "Not implemented.");
    } else {
        return luaL_error(L, "eg-overlay.clipboard_text([text]) takes either 1 or 0 arguments.");
    }
}

/*** RST
.. lua:function:: exit()

    Exit EG-Overlay.

    .. danger::
        This will unconditionally and immediately cause the overlay to shutdown. Most modules will not use this function.

    .. versionhistory::
        :0.0.1: Added
*/
static int overlay_exit(lua_State *L) {
    UNUSED_PARAM(L);
    
    app_exit();

    return 0;
}

/*** RST
Events
------

.. overlay:event:: startup

    The startup event is sent once before the start of the render thread. Modules can use this event to initialize or load data.

    .. versionhistory::
        :0.0.1: Added

.. overlay:event:: update

    Sent once per frame before any drawing has occurred.

    .. versionhistory::
        :0.0.1: Added

.. overlay:event:: draw-3d

    Sent once per frame, after :overlay:event:`update` but before the UI has been drawn. Modules should use this event to draw 3D information below the UI.

    .. versionhistory::
        :0.0.1: Added
*/