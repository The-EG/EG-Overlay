#pragma once
#include <lua.h>
#include <jansson.h>

void lua_manager_init();
void lua_manager_cleanup();

void lua_manager_run_file(const char *path);

char *lua_manager_get_lua_module_name(lua_State *L);
char *lua_manager_get_lua_module_name2(lua_State *L, int stack_depth);
char *lua_manager_get_lua_module_name_and_line(lua_State *L);
char *lua_manager_get_lua_module_name_and_line2(lua_State *L, int stack_depth);

typedef int lua_manager_event_callback(lua_State *L, void *data);

void lua_manager_add_event_callback(lua_manager_event_callback *cb, void *data);

void lua_manager_run_events();

void lua_manager_run_event_queue();

void lua_manager_queue_event(const char *event, json_t *data);
void lua_manager_run_event(const char *event, json_t *data);

typedef int lua_manager_module_opener_fn(lua_State *L);

void lua_manager_add_module_opener(const char *module_name, lua_manager_module_opener_fn *opener_fn);

int lua_manager_resume_coroutines();

void lua_manager_unref(int cbi);
int lua_manager_gettableref_bool(int table_ind, const char *field);
void lua_manager_settabletref_bool(int table_ind, const char *field, int value);
