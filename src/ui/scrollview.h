#pragma once
#include "ui.h"
#include <lua.h>

typedef struct ui_scroll_view_t ui_scroll_view_t;

ui_scroll_view_t *ui_scroll_view_new();

void ui_scroll_view_set_child(ui_scroll_view_t *scroll, ui_element_t *child);

void ui_scroll_view_register_lua_funcs(lua_State *L);

void ui_scroll_view_scroll_y(ui_scroll_view_t *scroll, int scroll_y);
