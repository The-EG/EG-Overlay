#pragma once
#include <jansson.h>
#include <lua.h>

void json_lua_init();

json_t *lua_checkjson(lua_State *L, int index);
void lua_pushjson(lua_State *L, json_t *json);