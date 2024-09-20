#pragma once
#include "color.h"
#include "font.h"
#include "lamath.h"
#include <lua.h>

typedef struct ui_text_s ui_text_t;
typedef struct ui_text_multi_t ui_text_multi_t;

ui_text_t *ui_text_new(const char *text, ui_color_t color, ui_font_t *font);

void ui_text_update_text(ui_text_t *text, const char *new_text);

void ui_text_lua_register_ui_funcs(lua_State *L);

void ui_text_set_pos(ui_text_t *text, int x, int y);
void ui_text_set_size(ui_text_t *text, int width, int height);
