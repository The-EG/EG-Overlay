#pragma once
#include "ui.h"

typedef struct ui_window_s ui_window_t;

ui_window_t *ui_window_new(const char *caption, int x, int y);

void ui_window_lua_register_ui_funcs(lua_State *L);
void lua_push_ui_window(lua_State *L, ui_window_t *window);

void ui_window_set_child(ui_window_t *window, ui_element_t *child);

void ui_window_show(ui_window_t *window);
void ui_window_hide(ui_window_t *window);

//void ui_window_set_resizable(ui_window_t *window, int resizable);
//void ui_window_set_autosize(ui_window_t *window, int autosize);