#include "web-request.h"
#include "logging/logger.h"
#include "utils.h"
#include "lua-manager.h"
#include <stdlib.h>
#include <windows.h>
#include <wininet.h>
#include <shlwapi.h>
#include <lua.h>
#include <lauxlib.h>
#include "eg-overlay.h"

typedef struct web_request_list_t {
    web_request_t *request;
    web_request_callback *cb;
    int cbi;
    int requesti;
    char *source;
    struct web_request_list_t *next;
} web_request_list_t;

typedef struct {
    HINTERNET internet;

    logger_t *log;
    HANDLE request_thread;
    int stop_thread;

    web_request_list_t *request_queue;
    HANDLE queue_mutex;
} web_request_static_t;

web_request_static_t *wr = NULL;

static DWORD WINAPI web_request_thread(LPVOID lpParam);

typedef struct web_request_value_list_t {
    char *name;
    char *value;
    struct web_request_value_list_t *next;
} web_request_value_list_t;

struct web_request_t {
    char *url;

    web_request_value_list_t *headers;
    web_request_value_list_t *query_params;

    int free_after_perform;
};

char *web_request_escape_url(const char *url);

void lua_pushwebrequest(lua_State *L, web_request_t *request, int lua_managed);
web_request_t *lua_checkwebrequest(lua_State *L, int ind);
int web_request_lua_open_module(lua_State *L);

void web_request_init() {
    wr = egoverlay_calloc(1, sizeof(web_request_static_t));
    wr->log = logger_get("web-request");

    logger_debug(wr->log, "init");

    wr->internet = InternetOpen(
        "EG-Overlay/" VERSION_STR,
        INTERNET_OPEN_TYPE_PRECONFIG,
        NULL, NULL, 0
    );

    if (wr->internet==NULL) {
        logger_error(wr->log, "Couldn't initialize WinInet.");
        error_and_exit("EG-Overlay: Web Request", "Couldn't initialize WinInet.");
    }

    wr->queue_mutex = CreateMutex(0, FALSE, NULL);
    if (wr->queue_mutex==NULL) {
        logger_error(wr->log, "Couldn't create request queue mutex.");
        error_and_exit("EG-Overlay: Web Request", "Couldn't create request queue mutex.");
    }

    wr->request_thread = CreateThread(0, 0, &web_request_thread, NULL, 0, NULL);
    if (wr->request_thread==NULL) {
        logger_error(wr->log, "Couldn't create request thread.");
        error_and_exit("EG-Overlay: Web Request", "Couldn't create request thread.");
    }

    lua_manager_add_module_opener("web-request", &web_request_lua_open_module);
}

void web_request_cleanup() {
    logger_debug(wr->log, "cleanup");

    wr->stop_thread = 1;
    WaitForSingleObject(wr->request_thread, INFINITE);
    CloseHandle(wr->request_thread);

    CloseHandle(wr->queue_mutex);

    InternetCloseHandle(wr->internet);

    egoverlay_free(wr);
}

struct web_request_lua_callback_data {
    web_request_t *req;
    int reqi;
    int cbi;
    long http_code;
    char *data;
};

static int web_request_run_lua_callback(lua_State *L, struct web_request_lua_callback_data *data) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, data->cbi);

    lua_pushinteger(L, data->http_code);
    lua_pushstring(L, data->data);
    lua_pushwebrequest(L, data->req, 0);

    // remove our references to the callback and request
    luaL_unref(L, LUA_REGISTRYINDEX, data->cbi);
    luaL_unref(L, LUA_REGISTRYINDEX, data->reqi);

    egoverlay_free(data->data);
    egoverlay_free(data);

    return 3;
}

char *web_request_escape_url(const char *url) {
    DWORD urllen = 1;
    char *outurl = egoverlay_calloc(1, sizeof(char));

    if (UrlEscape(url, outurl, &urllen, 0)==E_POINTER) {
        outurl = egoverlay_realloc(outurl, (urllen+1) * sizeof(char));

        if (UrlEscape(url, outurl, &urllen, 0)!=S_OK) {
            egoverlay_free(outurl);
            return NULL;
        }

        return outurl;
    }

    egoverlay_free(outurl);
    return NULL;
}

void call_lua_cb(web_request_list_t *req, int http_code, const char *data) {
    size_t ldsize = sizeof(struct web_request_lua_callback_data);
    struct web_request_lua_callback_data *ld = egoverlay_calloc(1, ldsize);
    ld->cbi = req->cbi;
    if (data) {
        ld->data = egoverlay_calloc(strlen(data)+1, sizeof(char));
        memcpy(ld->data, data, strlen(data));
    }
    ld->req = req->request;
    ld->reqi = req->requesti;
    ld->http_code = http_code;
    lua_manager_add_event_callback(&web_request_run_lua_callback, ld);
}

static void web_request_perform(web_request_list_t *req) {
    web_request_t *request = req->request;

    size_t comburllen = strlen(request->url);
    for (web_request_value_list_t *v = request->query_params;v;v=v->next) {
        comburllen += strlen(v->name) + strlen(v->value) + 2;
    }

    char *comburl = egoverlay_calloc(comburllen+1, sizeof(char));
    memcpy(comburl, request->url, strlen(request->url));
    
    size_t cind = strlen(request->url);
    for (web_request_value_list_t *v = request->query_params;v;v=v->next) {
        if (cind==strlen(request->url)) comburl[cind] = '?';
        else comburl[cind] = '&';
        cind++;

        memcpy(comburl + cind, v->name, strlen(v->name));
        cind += strlen(v->name);
        
        comburl[cind] = '=';
        cind++;
        
        memcpy(comburl + cind, v->value, strlen(v->value));
        cind += strlen(v->value);
    }

    char *url = web_request_escape_url(comburl);
    egoverlay_free(comburl);

    if (!url) {
        logger_error(wr->log, "Bad URL: %s", request->url);
        if (req->cb) req->cb(-1, "bad URL", request);
        if (req->cbi) call_lua_cb(req, -1, "bad URL");
        return;
    }

    size_t hdrslen = 0;
    char *hdrs = NULL;
    for (web_request_value_list_t *v = request->headers;v;v=v->next) {
        size_t hlen = 4 + strlen(v->name) + strlen(v->value);
        hdrs = egoverlay_realloc(hdrs, hdrslen + hlen);

        memcpy(hdrs + hdrslen, v->name, strlen(v->name));
        hdrslen += strlen(v->name);
        hdrs[hdrslen++] = ':';
        hdrs[hdrslen++] = ' ';
        memcpy(hdrs + hdrslen, v->value, strlen(v->value));
        hdrslen += strlen(v->value);
        hdrs[hdrslen++] = '\r';
        hdrs[hdrslen++] = '\n';
    }

    HINTERNET hreq = InternetOpenUrl(wr->internet, url, hdrs, (DWORD)hdrslen, 0, (DWORD_PTR)NULL);

    if (hdrs) egoverlay_free(hdrs);

    if (!hreq) {
        logger_error(wr->log, "Couldn't open URL: %s", url);
        if (req->cb) req->cb(-1, "Couldn't open URL", request);
        if (req->cbi) call_lua_cb(req, -1, "Couldn't open URL");
        egoverlay_free(url);
        return;
    }

    char *bytes = NULL;
    size_t byteslen = 0;

    char chunk[1024] = {0};
    size_t read = 0;

    while (InternetReadFile(hreq, chunk, 1024, (LPDWORD)&read)) {
        if (read==0) break;
        bytes = egoverlay_realloc(bytes, sizeof(char) * (byteslen + read));
        memcpy(bytes + byteslen, chunk, read);
        byteslen += read;
    }

    bytes = egoverlay_realloc(bytes, sizeof(char) * (byteslen+1));
    bytes[byteslen] = '\0';

    uint32_t statuscode = 0;
    size_t codelen = sizeof(uint32_t);
    HttpQueryInfo(hreq, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statuscode, (LPDWORD)&codelen, NULL);

    InternetCloseHandle(hreq);

    if (statuscode>=200 && statuscode<400) {
        if (req->source) logger_info(wr->log, "%s: GET %s -> %d", req->source, url, statuscode);
        else logger_info(wr->log, "GET %s -> %d", url, statuscode);
    } else {
        if (req->source) logger_warn(wr->log, "%s: GET %s -> %d", req->source, url, statuscode);
        else logger_warn(wr->log, "GET %s -> %d", url, statuscode);
    }

    egoverlay_free(url);

    if (req->cb) req->cb(statuscode, NULL, request);
    if (req->cbi) {
        size_t ldsize = sizeof(struct web_request_lua_callback_data);
        struct web_request_lua_callback_data *ld = egoverlay_calloc(1, ldsize);
        ld->cbi = req->cbi;
        if (bytes) {
            ld->data = egoverlay_calloc(strlen(bytes)+1, sizeof(char));
            memcpy(ld->data, bytes, strlen(bytes));
        }
        ld->req = request;
        ld->reqi = req->requesti;
        ld->http_code = statuscode;
        lua_manager_add_event_callback(&web_request_run_lua_callback, ld);
    }

    egoverlay_free(bytes);

    if (request->free_after_perform) web_request_free(request);
}

static DWORD WINAPI web_request_thread(LPVOID lpParam) {
    UNUSED_PARAM(lpParam);
    
    logger_debug(wr->log, "request thread starting...");
    while(!wr->stop_thread) {
        WaitForSingleObject(wr->queue_mutex, INFINITE);

        web_request_list_t *r = wr->request_queue;

        wr->request_queue = NULL;

        // we can release the mutex now, any new requests that get queued
        // while we are performing the current queue will just be added
        // to a new list
        ReleaseMutex(wr->queue_mutex);

        while (r) {
            web_request_perform(r);
            web_request_list_t *prev = r;
            r = r->next;
            if (prev->source) egoverlay_free(prev->source);
            egoverlay_free(prev);
        }
        Sleep(25);
    }
    logger_debug(wr->log, "request thread ending...");

    return 0;
}


web_request_t *web_request_new(const char *url) {
    web_request_t *r = egoverlay_calloc(1, sizeof(web_request_t));

    r->url = egoverlay_calloc(strlen(url) + 1, sizeof(char));
    memcpy(r->url, url, strlen(url));

    return r;
}

void web_request_free(web_request_t *request) {
    egoverlay_free(request->url);

    web_request_value_list_t *v = request->headers;
    web_request_value_list_t *n = NULL;
    while (v) {
        egoverlay_free(v->name);
        egoverlay_free(v->value);
        n = v->next;
        egoverlay_free(v);
        v = n;
    }

    v = request->query_params;
    n = NULL;

    while (v) {
        egoverlay_free(v->name);
        egoverlay_free(v->value);
        n = v->next;
        egoverlay_free(v);
        v = n;
    }

    egoverlay_free(request);
}

static web_request_value_list_t *web_request_value_list_new_item(const char *name, const char *value) {
    web_request_value_list_t *v = egoverlay_calloc(1, sizeof(web_request_value_list_t));
    v->name = egoverlay_calloc(strlen(name)+1, sizeof(char));
    memcpy(v->name, name, strlen(name));
    v->value = egoverlay_calloc(strlen(value)+1, sizeof(char));
    memcpy(v->value, value, strlen(value));

    return v;
}

static void web_request_value_list_add_item(web_request_value_list_t **list, web_request_value_list_t *item) {
    if (*list==NULL) {
        *list = item;
        return;
    }

    web_request_value_list_t *h = *list;
    while (h->next) h = h->next;

    h->next = item;   
}

void web_request_add_header(web_request_t *request, const char *name, const char* value) {
    web_request_value_list_t *v = web_request_value_list_new_item(name, value);
    web_request_value_list_add_item(&request->headers, v);
}

void web_request_add_query_parameter(web_request_t *request, const char *name, const char *value) {
    web_request_value_list_t *v = web_request_value_list_new_item(name, value);
    web_request_value_list_add_item(&request->query_params, v);
}

void web_request_queue(
    web_request_t *request,
    web_request_callback *callback,
    int free_after,
    const char *source,
    int cbi
) {
    web_request_list_t *w = egoverlay_calloc(1, sizeof(web_request_list_t));
    w->request = request;
    w->cb = callback;
    w->cbi = cbi;

    if (source) {
        w->source = egoverlay_calloc(strlen(source)+1, sizeof(char));
        memcpy(w->source, source, strlen(source));
    }

    request->free_after_perform = free_after;

    WaitForSingleObject(wr->queue_mutex, INFINITE);

    if (wr->request_queue==NULL) {
        wr->request_queue = w;
    } else {
        web_request_list_t *last = wr->request_queue;
        while (last->next) last = last->next;
        last->next = w;
    }

    ReleaseMutex(wr->queue_mutex);
}

int web_request_lua_new(lua_State *L);
int web_request_lua_del(lua_State *L);
int web_request_lua_add_header(lua_State *L);
int web_request_lua_add_query_parameter(lua_State *L);
int web_request_lua_queue(lua_State *L);

/*** RST
web-request
===========

.. lua:module:: web-request

.. code-block:: lua

    local request = require 'web-request'

The :lua:mod:`web-request` module provides a method to make asynchronous http(s)
requests. Due to EG-Overlay's Lua and render thread setup, synchronous requests
are not supported.

All requests are performed on a dedicated thread and results are queued as
events back to Lua. This means that a module that queues a request during a Lua
event will not receive a response until the following frame/events run.

.. important::
    All requests are logged at the ``INFO`` level and include the file and line
    number where the request was queued. This is intended to give EG-Overlay
    users full transparency and awareness of what web requests are being made,
    to where, and which modules are making them.

    Module authors that use the :lua:mod:`web-request` module directly should
    also consider caching results so that multiple requests for the same data
    are not completed.
*/

int web_request_lua_open_module(lua_State *L) {
    lua_newtable(L);
    lua_pushcfunction(L, &web_request_lua_new);
    lua_setfield(L, -2, "new");

    return 1;
}

luaL_Reg web_request_lua_funcs[] = {
    "__gc"             , &web_request_lua_del,
    "addheader"        , &web_request_lua_add_header,
    "addqueryparameter", &web_request_lua_add_query_parameter,
    "queue"            , &web_request_lua_queue,
    NULL               ,  NULL
};

void web_request_lua_register_metatable(lua_State *L) {
    if (luaL_newmetatable(L, "WebRequestMetaTable")) {
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");

        luaL_setfuncs(L, web_request_lua_funcs, 0);
    }
}

void lua_pushwebrequest(lua_State *L, web_request_t *request, int lua_managed) {
    web_request_t **req = (web_request_t**)lua_newuserdata(L, sizeof(web_request_t*));

    *req = request;

    lua_pushboolean(L, lua_managed);
    lua_setiuservalue(L, -2, 1);
    web_request_lua_register_metatable(L);
    lua_setmetatable(L, -2);
}

web_request_t *lua_checkwebrequest(lua_State *L, int ind) {
    return *(web_request_t**)luaL_checkudata(L, ind, "WebRequestMetaTable");

}

/*** RST
Functions
---------

.. lua:function:: new(url)

    Creates a new :lua:class:`web-request.webrequest`.

    :param url: The URL to make the request to.
    :type url: string
    :rtype: web-request.webrequest

    .. versionhistory::
        :0.0.1: Added
*/
int web_request_lua_new(lua_State *L) {
    const char *url = luaL_checkstring(L, 1);
    web_request_t *req = web_request_new(url);

    lua_pushwebrequest(L, req, 1);

    return 1;
}

int web_request_lua_del(lua_State *L) {
    web_request_t *r = lua_checkwebrequest(L, 1);

    lua_getiuservalue(L, -1, 1);
    int lua_managed = lua_toboolean(L, -1);
    lua_pop(L, 1);

    if (lua_managed) {
        web_request_free(r);
    }

    return 0;
}

/*** RST
Classes
-------

.. lua:class:: webrequest

    A web request object. A single web request object can be used to make
    multiple requests to the same URL.

    .. lua:method:: addheader(name, value)

        Add a custom header to this request. All subsequent requests made by
        this webrequest will include the header.

        :param name: The header name, ie. ``'Authorization'``
        :type name: string
        :param value: The header value.
        :type value: string
        :return: none

        .. code-block:: lua
            :caption: Example

            request:addheader('Authorization', 'Bearer (API Key)')

        .. versionhistory::
            :0.0.1: Added
            :0.1.0: Renamed from add_header to addheader
*/
int web_request_lua_add_header(lua_State *L) {
    web_request_t *r = lua_checkwebrequest(L, 1);
    const char *name = luaL_checkstring(L, 2);
    const char *value = luaL_checkstring(L, 3);

    web_request_add_header(r, name, value);

    return 0;
}

/*** RST
    .. lua:method:: addqueryparameter(name, value)

        Add a query parameter that will be appended to the URL when making the
        request.

        :param name: The parameter name.
        :type name: string
        :param value: The parameter value.
        :type value: string
        :return: none

        .. versionhistory::
            :0.0.1: Added
            :0.1.0: Renamed from add_query_parameter to addqueryparameter
*/
int web_request_lua_add_query_parameter(lua_State *L) {
    web_request_t *r = lua_checkwebrequest(L, 1);
    const char *name = luaL_checkstring(L, 2);
    const char *value = luaL_checkstring(L, 3);

    web_request_add_query_parameter(r, name, value);

    return 0;
}

/*** RST
    .. lua:method:: queue(callback_function)

        Queue the request to be performed. A :lua:class:`webrequest` can be
        queued multiple times to perform the request multiple times to the same URL.

        ``callback_function`` is a :lua:alias:`request_completed` function that
        is called when the request is completed.

        :param callback_function: A function that will be called when the
            request has been completed.
        :type callback_function: request_completed
        :return: none
*/
int web_request_lua_queue(lua_State *L) {
    web_request_t *r = lua_checkwebrequest(L, 1);

    web_request_list_t *w = egoverlay_calloc(1, sizeof(web_request_list_t));
    w->request = r;

    // save a reference to the request on the Lua side. This will keep
    // Lua from GCing it before we process it.
    lua_pushvalue(L, 1);
    w->requesti = luaL_ref(L, LUA_REGISTRYINDEX);

    // the callback function
    lua_pushvalue(L, 2);
    w->cbi = luaL_ref(L, LUA_REGISTRYINDEX);

    int stack_depth = 1;
    if (lua_gettop(L)==3) {
        // a third 'undocumented' argument, how far back in the stack to go
        // for logging which file/line called this. intermediary modules can use
        // this so that the log reports where that module was called and not itself
        stack_depth = (int)luaL_checkinteger(L, 3);
    }

    char *mod_name = lua_manager_get_lua_module_name_and_line2(L, stack_depth);
    size_t mod_name_len = strlen(mod_name);

    w->source = egoverlay_calloc(mod_name_len+1, sizeof(char));
    memcpy(w->source, mod_name, mod_name_len);
    egoverlay_free(mod_name);

    WaitForSingleObject(wr->queue_mutex, INFINITE);

    if (wr->request_queue==NULL) {
        wr->request_queue = w;
    } else {
        web_request_list_t *last = wr->request_queue;
        while (last->next) last = last->next;
        last->next = w;
    }

    ReleaseMutex(wr->queue_mutex);
    
    return 0;
}

/*** RST
Callback Functions
------------------

.. lua:currentmodule:: None

.. lua:function:: request_completed(code, data, request)

    A callback function provided to :lua:meth:`web-request.webrequest.queue`,
    called when the request is completed.

    :param code: The HTTP response code or 0 if an error occurred before the
        request could be made.
    :type code: integer
    :param data: The response body returned by the request. For responses that
        resulted in HTTP error codes this will contain the error text returned
        by the server.
    :type data: string
    :param request: The :lua:class:`web-request.webrequest` that made the request.
    :type request: web-request.webrequest

    .. versionhistory::
        :0.0.1: Added


Example
-------

.. note::
    Module authors should use the bundled :lua:mod:`gw2.api` module to make API
    requests. These examples are provided to show how to use this module.

.. code-block:: lua

    local wr = require 'web-request'
    local JSON = require 'JSON'

    local api_url = 'https://api.guildwars2.com/v2/'

    local req = wr.new(api_url .. 'account')

    req:addheader('Authorization', 'Bearer (apikey)')
    req:addheader('X-Schema-Version', 'latest')

    local function on_completed(code, data, r)
        if code >= 200 && code < 400 then
            local account_data = JSON.parse_string(data)
            print(string.format("Account age: %f days", account_data.age / 60.0 / 24.0))
        else
            if code > 0 then
                print(string.format("Couldn't complete request, got %d: %s", code, data))
            else
                print("Couldn't complete request.")
            end
        end
    end

    req:queue(on_completed)
*/
