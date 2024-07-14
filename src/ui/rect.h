#pragma once
#include "color.h"
#include <stdint.h>
#include "lamath.h"

typedef struct ui_rect_s ui_rect_t;

void ui_rect_init();
void ui_rect_cleanup();

void ui_rect_lua_register_ui_funcs(lua_State *L);

void ui_rect_draw(int x, int y, int width, int height, ui_color_t color, mat4f_t *proj);

