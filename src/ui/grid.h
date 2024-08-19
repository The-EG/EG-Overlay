#pragma once
#include <lua.h>

typedef struct ui_grid_t ui_grid_t;

ui_grid_t *ui_grid_new(int rows, int cols);

void ui_grid_attach(ui_grid_t *grid, void *uielement, int row, int col, int rowspan, int colspan, int horizalign, int vertalign);
void ui_grid_rowspacing(ui_grid_t *grid, int row, int spacing);
void ui_grid_colspacing(ui_grid_t *grid, int col, int spacing);

void ui_grid_lua_register_ui_funcs(lua_State *L);