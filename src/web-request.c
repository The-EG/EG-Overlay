#include "web-request.h"
#include "logging/logger.h"
#include "utils.h"
#include "lua-manager.h"
#include <stdlib.h>
#include <curl/curl.h>
#include <windows.h>
#include <lua.h>
#include <lauxlib.h>

static CURL *curl = NULL;
static logger_t *logger = NULL;
static HANDLE request_thread = NULL;
static DWORD request_thread_id = 0;
static int stop_thread = 0;

typedef struct web_request_list_t {
    web_request_t *request;
    web_request_callback *cb;
    int cbi;
    int requesti;
    char *source;
    struct web_request_list_t *next;
} web_request_list_t;

static web_request_list_t *request_queue = NULL;
HANDLE queue_mutex = NULL;

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

static void web_request_push_to_lua(lua_State *L, web_request_t *request, int lua_managed);
int web_request_lua_open_module(lua_State *L);

void web_request_init() {
    logger = logger_get("web-request");

    if (curl_global_init(CURL_GLOBAL_DEFAULT)!=CURLE_OK) {
        logger_error(logger, "Error while performing curl_global_init.");
        error_and_exit("EG-Overlay: Web Request", "Error while performing curl_global_init.");
    }

    curl = curl_easy_init();
    if (curl==NULL) {
        logger_error(logger, "Error while performing curl_easy_init.");
        error_and_exit("EG-Overlay: Web Request", "Error while performing curl_easy_init.");
    }

    queue_mutex = CreateMutex(0, FALSE, NULL);
    if (queue_mutex==NULL) {
        logger_error(logger, "Couldn't create request queue mutex.");
        error_and_exit("EG-Overlay: Web Request", "Couldn't create request queue mutex.");
    }

    request_thread = CreateThread(0, 0, &web_request_thread, NULL, 0, &request_thread_id);
    if (request_thread==NULL) {
        logger_error(logger, "Couldn't create request thread.");
        error_and_exit("EG-Overlay: Web Request", "Couldn't create request thread.");
    }

    lua_manager_add_module_opener("web-request", &web_request_lua_open_module);
}

void web_request_cleanup() {
    stop_thread = 1;
    WaitForSingleObject(request_thread, INFINITE);
    CloseHandle(request_thread);

    CloseHandle(queue_mutex);

    curl_easy_cleanup(curl);
    curl_global_cleanup();
}

static size_t web_request_write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    UNUSED_PARAM(size);

    // naive but should work for text response data just fine

    char **data = (char**)userdata;
    size_t cur_len = (*data) ? strlen(*data) : 0;
    size_t new_len = cur_len + nmemb + 1;

    *data = realloc(*data, new_len);
    //if (cur_len) memcpy(new_data, *data, cur_len);
    memcpy((*data) + cur_len, ptr, nmemb);
    (*data)[new_len-1] = 0;

    //free(*data);
    //*data = new_data;

    return nmemb;
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
    web_request_push_to_lua(L, data->req, 0);

    /*
    if (lua_pcall(L, 3, 0, 0)!=LUA_OK) {
        const char *errmsg = luaL_checkstring(L, -1);
        logger_error(logger, "Error occured during web request Lua callback: %s", errmsg);
        lua_pop(L, 1);
    }
    */

    // remove our references to the callback and request
    luaL_unref(L, LUA_REGISTRYINDEX, data->cbi);
    luaL_unref(L, LUA_REGISTRYINDEX, data->reqi);

    free(data->data);
    free(data);

    return 3;
}

static void web_request_perform(web_request_list_t *req) {
    web_request_t *request = req->request;
    curl_easy_reset(curl);

    CURLU *url = curl_url();
    curl_url_set(url, CURLUPART_URL, request->url, CURLU_URLENCODE);

    web_request_value_list_t *v = request->query_params;
    while (v) {
        size_t query_param_size = strlen(v->name) + strlen(v->value) + 1;
        char *query_param = calloc(query_param_size + 1, sizeof(char));
        
        memcpy(query_param, v->name, strlen(v->name));
        query_param[strlen(v->name)] = '=';
        memcpy(query_param + strlen(v->name)+1, v->value, strlen(v->value));

        curl_url_set(url, CURLUPART_QUERY, query_param, CURLU_URLENCODE | CURLU_APPENDQUERY);

        free(query_param);

        v = v->next;
    }

    curl_easy_setopt(curl, CURLOPT_CURLU, url);

    struct curl_slist *hdrs = NULL;

    v = request->headers;
    while (v) {
        size_t header_size = strlen(v->name) + strlen(v->value) + 2;
        char *header = calloc(header_size + 1, sizeof(char));

        memcpy(header, v->name, strlen(v->name));
        memcpy(header + strlen(v->name), ": ", 2);
        memcpy(header + strlen(v->name) + 2, v->value, strlen(v->value));

        hdrs = curl_slist_append(hdrs, header);

        free(header);

        v = v->next;
    }

    if (hdrs) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);

    char *data = NULL;

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &web_request_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&data);

    CURLcode res = curl_easy_perform(curl);

    if (res==CURLE_OK) {
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        if (http_code>=200 && http_code<400) {
            if (req->source) logger_info(logger, "%s: GET %s -> %d", req->source, request->url, http_code);
            else logger_info(logger, "GET %s -> %d", request->url, http_code);
        } else {
            if (req->source) logger_warn(logger, "%s: GET %s -> %d", req->source, request->url, http_code);
            else logger_warn(logger, "GET %s -> %d", request->url, http_code);
        }

        if (req->cb) req->cb(http_code, data, request);
        if (req->cbi){
            struct web_request_lua_callback_data *ld = calloc(1, sizeof(struct web_request_lua_callback_data));
            ld->cbi = req->cbi;
            ld->data = calloc(strlen(data)+1, sizeof(char));
            memcpy(ld->data, data, strlen(data));
            ld->req = request;
            ld->reqi = req->requesti;
            ld->http_code = http_code;
            lua_manager_add_event_callback(&web_request_run_lua_callback, ld);
        }
    } else {
        logger_error(logger, "Error while performing GET to %s: %s", request->url, curl_easy_strerror(res));
    }

    curl_url_cleanup(url);

    if (data) free(data);

    if (hdrs) curl_slist_free_all(hdrs);

    if (request->free_after_perform) web_request_free(request);
}

static DWORD WINAPI web_request_thread(LPVOID lpParam) {
    UNUSED_PARAM(lpParam);
    
    logger_debug(logger, "request thread starting...");
    while(!stop_thread) {
        WaitForSingleObject(queue_mutex, INFINITE);

        web_request_list_t *r = request_queue;

        request_queue = NULL;

        // we can release the mutex now, any new requests that get queued
        // while we are performing the current queue will just be added
        // to a new list
        ReleaseMutex(queue_mutex);

        while (r) {
            web_request_perform(r);
            web_request_list_t *prev = r;
            r = r->next;
            if (prev->source) free(prev->source);
            free(prev);
        }
        Sleep(25);
    }
    logger_debug(logger, "request thread ending...");

    return 0;
}


web_request_t *web_request_new(const char *url) {
    web_request_t *r = calloc(1, sizeof(web_request_t));

    r->url = calloc(strlen(url) + 1, sizeof(char));
    memcpy(r->url, url, strlen(url));

    return r;
}

void web_request_free(web_request_t *request) {
    free(request->url);

    web_request_value_list_t *v = request->headers;
    web_request_value_list_t *n = NULL;
    while (v) {
        free(v->name);
        free(v->value);
        n = v->next;
        free(v);
        v = n;
    }

    v = request->query_params;
    n = NULL;

    while (v) {
        free(v->name);
        free(v->value);
        n = v->next;
        free(v);
        v = n;
    }

    free(request);
}

static web_request_value_list_t *web_request_value_list_new_item(const char *name, const char *value) {
    web_request_value_list_t *v = calloc(1, sizeof(web_request_value_list_t));
    v->name = calloc(strlen(name)+1, sizeof(char));
    memcpy(v->name, name, strlen(name));
    v->value = calloc(strlen(value)+1, sizeof(char));
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

void web_request_queue(web_request_t *request, web_request_callback *callback, int free_after, const char *source, int cbi) {
    web_request_list_t *w = calloc(1, sizeof(web_request_list_t));
    w->request = request;
    w->cb = callback;
    w->cbi = cbi;

    if (source) {
        w->source = calloc(strlen(source)+1, sizeof(char));
        memcpy(w->source, source, strlen(source));
    }

    request->free_after_perform = free_after;

    WaitForSingleObject(queue_mutex, INFINITE);

    if (request_queue==NULL) {
        request_queue = w;
    } else {
        web_request_list_t *last = request_queue;
        while (last->next) last = last->next;
        last->next = w;
    }

    ReleaseMutex(queue_mutex);
}

static int web_request_lua_new(lua_State *L);
static int web_request_lua_del(lua_State *L);
static int web_request_lua_add_header(lua_State *L);
static int web_request_lua_add_query_parameter(lua_State *L);
static int web_request_lua_queue(lua_State *L);

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

static luaL_Reg web_request_lua_funcs[] = {
    "__gc",                &web_request_lua_del,
    "add_header",          &web_request_lua_add_header,
    "add_query_parameter", &web_request_lua_add_query_parameter,
    "queue",               &web_request_lua_queue,
    NULL,                   NULL
};

static void web_request_lua_register_metatable(lua_State *L) {
    if (luaL_newmetatable(L, "WebRequestMetaTable")) {
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");

        luaL_setfuncs(L, web_request_lua_funcs, 0);
    }
}

#define LUA_CHECK_WEBREQUEST(L, i) *(web_request_t**)luaL_checkudata(L, i, "WebRequestMetaTable")

static void web_request_push_to_lua(lua_State *L, web_request_t *request, int lua_managed) {
    web_request_t **req = (web_request_t**)lua_newuserdata(L, sizeof(web_request_t*));

    *req = request;

    lua_pushboolean(L, lua_managed);
    lua_setiuservalue(L, -2, 1);
    web_request_lua_register_metatable(L);
    lua_setmetatable(L, -2);
}

/*** RST
Functions
---------

.. lua:function:: new(url)

    Creates a new :lua:class:`web-request.web_request`.

    :param url: The URL to make the request to.
    :type url: string
    :rtype: web-request.web_request

    .. versionhistory::
        :0.0.1: Added
*/
static int web_request_lua_new(lua_State *L) {
    const char *url = luaL_checkstring(L, 1);
    web_request_t *req = web_request_new(url);

    web_request_push_to_lua(L, req, 1);

    return 1;
}

static int web_request_lua_del(lua_State *L) {
    web_request_t *r = LUA_CHECK_WEBREQUEST(L, 1);

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

.. lua:class:: web_request

    A web request object. A single web request object can be used to make multiple requests to the same URL.

    .. lua:method:: add_header(name, value)

        Add a custom header to this request. All subsequent requests made by this web_request will include the header.

        :param name: The header name, ie. ``'Authorization'``
        :type name: string
        :param value: The header value.
        :type value: string
        :return: none

        .. code-block:: lua
            :caption: Example

            request:add_header('Authorization', 'Bearer (API Key)')

        .. versionhistory::
            :0.0.1: Added
*/
static int web_request_lua_add_header(lua_State *L) {
    web_request_t *r = LUA_CHECK_WEBREQUEST(L, 1);
    const char *name = luaL_checkstring(L, 2);
    const char *value = luaL_checkstring(L, 3);

    web_request_add_header(r, name, value);

    return 0;
}

/*** RST
    .. lua:method:: add_query_parameter(name, value)

        Add a query parameter that will be appended to the URL when making the request.

        :param name: The parameter name.
        :type name: string
        :param value: The parameter value.
        :type value: string
        :return: none

        .. versionhistory::
            :0.0.1: Added
*/
static int web_request_lua_add_query_parameter(lua_State *L) {
    web_request_t *r = LUA_CHECK_WEBREQUEST(L, 1);
    const char *name = luaL_checkstring(L, 2);
    const char *value = luaL_checkstring(L, 3);

    web_request_add_query_parameter(r, name, value);

    return 0;
}

/*** RST
    .. lua:method:: queue(callback_function)

        Queue the request to be performed. A :lua:class:`web_request` can be queued multiple times to perform the request multiple times to the same URL.

        ``callback_function`` is a :lua:alias:`request_completed` function that is called when the request is completed.

        :param callback_function: A function that will be called when the request has been completed.
        :type callback_function: request_completed
        :return: none
*/
static int web_request_lua_queue(lua_State *L) {
    web_request_t *r = LUA_CHECK_WEBREQUEST(L, 1);

    web_request_list_t *w = calloc(1, sizeof(web_request_list_t));
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

    w->source = calloc(mod_name_len+1, sizeof(char));
    memcpy(w->source, mod_name, mod_name_len);
    free(mod_name);

    WaitForSingleObject(queue_mutex, INFINITE);

    if (request_queue==NULL) {
        request_queue = w;
    } else {
        web_request_list_t *last = request_queue;
        while (last->next) last = last->next;
        last->next = w;
    }

    ReleaseMutex(queue_mutex);
    
    return 0;
}

/*** RST
Callback Functions
------------------

.. lua:currentmodule:: None

.. lua:function:: request_completed(code, data, request)

    A callback function provided to :lua:meth:`web-request.web_request.queue`, called when the request is completed.

    :param code: The HTTP response code or 0 if an error occurred before the request could be made.
    :type code: integer
    :param data: The response body returned by the request. For responses that resulted in HTTP error codes this will contain the error text returned by the server.
    :type data: string
    :param request: The :lua:class:`web-request.web_request` that made the request.
    :type request: web-request.web_request

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

    req:add_header('Authorization', 'Bearer (apikey)')
    req:add_header('X-Schema-Version', 'latest')

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