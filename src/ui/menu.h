#pragma once
#include <lua.h>
#include "color.h"
#include "font.h"
typedef struct ui_menu_t ui_menu_t;
typedef struct ui_menu_item_t ui_menu_item_t;

typedef void menu_item_event_callback(const char *event_type);

void ui_menu_item_set_event_callback(ui_menu_item_t *item, menu_item_event_callback *cb);

void ui_menu_item_set_sub_menu(ui_menu_item_t *item, ui_menu_t *menu);

ui_menu_t *ui_menu_new();

ui_menu_item_t *ui_menu_item_new();

void ui_menu_show(ui_menu_t *menu);
void ui_menu_hide(ui_menu_t *menu);

//void ui_menu_set_pos(ui_menu_t *menu, int x, int y);

void ui_menu_add_item(ui_menu_t *menu, ui_menu_item_t *item);

void ui_menu_lua_register_ui_funcs(lua_State *L);