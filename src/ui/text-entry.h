#pragma once
#include "font.h"
#include <lua.h>

typedef struct ui_text_entry_t ui_text_entry_t;

ui_text_entry_t *ui_text_entry_new(ui_font_t *font);

const char *ui_text_entry_get_text(ui_text_entry_t *entry);
void ui_text_entry_set_text(ui_text_entry_t *entry, const char *text);

void ui_text_entry_lua_register_funcs(lua_State *L);

void lua_push_ui_textentry(lua_State *L, ui_text_entry_t *entry);