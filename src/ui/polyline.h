#pragma once
#include <lua.h>
#include <stdint.h>
#include "color.h"
#include "../lamath.h"

typedef struct ui_polyline_t ui_polyline_t;

void ui_polyline_init();
void ui_polyline_cleanup();

ui_polyline_t *ui_polyline_new();
void ui_polyline_free(ui_polyline_t *line);

void ui_polyline_set_color(ui_polyline_t *line, ui_color_t color);
void ui_polyline_set_width(ui_polyline_t *line, uint8_t width);

void ui_polyline_add_point(ui_polyline_t *line, int x, int y);
void ui_polyline_clear_points(ui_polyline_t *line);
void ui_polyline_update(ui_polyline_t *line);

void ui_polyline_lua_register_ui_funcs(lua_State *L);
void ui_polyline_push_to_lua(ui_polyline_t *line, lua_State *L, int lua_managed);