#pragma once
#include "ui.h"
#include <lua.h>

typedef struct ui_box_t ui_box_t;

typedef enum {
    UI_BOX_ORIENTATION_VERTICAL,
    UI_BOX_ORIENTATION_HORIZONTAL
} ui_box_orientation_e;

ui_box_t *ui_box_new(ui_box_orientation_e orientation);

void ui_box_pack_end(ui_box_t *box, ui_element_t *element, int expand, int align);

void ui_box_set_padding(ui_box_t *box, int left, int right, int top, int bottom);

void ui_box_lua_register_ui_funcs(lua_State *L);