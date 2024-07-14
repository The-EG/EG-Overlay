#pragma once
#include "ui.h"

typedef struct ui_button_t ui_button_t;

ui_button_t *ui_button_new();
void ui_button_set_child(ui_button_t *button, ui_element_t *child);

void ui_button_lua_register_ui_funcs(lua_State *L);

ui_button_t *ui_checkbox_new(int size);