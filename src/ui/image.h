#pragma once
#include <lua.h>

void ui_image_init();
void ui_image_cleanup();

void ui_image_lua_register_ui_funcs(lua_State *L);
