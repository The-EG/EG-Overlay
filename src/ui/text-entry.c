
#include <stdlib.h>
#include <string.h>
#include <lauxlib.h>
#include "text-entry.h"
#include "ui.h"
#include "rect.h"
#include "../app.h"
#include "../utils.h"
#include "../logging/logger.h"
#include "../lua-manager.h"
#include <GLFW/glfw3.h>

#define MAX_TEXT_LEN 512

struct ui_text_entry_t {
    ui_element_t element;

    ui_font_t *font;

    size_t text_len;
    char *text;

    char *hint;

    int pref_height;
    int pref_width;

    double caret_counter;
    int caret_blink;
    int caret_pos;
    int caret_x;

    int lua_cbi;
};

void ui_text_entry_free(ui_text_entry_t *entry);

void ui_text_entry_draw(ui_text_entry_t *entry, int offset_x, int offset_y, mat4f_t *proj);
int ui_text_entry_get_preferred_size(ui_text_entry_t *entry, int *width, int *height);
int ui_text_entry_process_mouse_event(ui_text_entry_t *entry, ui_mouse_event_t *event, int offset_x, int offset_y);
int ui_text_entry_process_keyboard_event(ui_text_entry_t *entry, ui_keyboard_event_t *event);

typedef struct {
    ui_text_entry_t *entry;
    char *key_name;
} keydown_event_data_t;

int ui_text_lua_keydown_event_run_callback(lua_State *L, keydown_event_data_t *data);

ui_text_entry_t *ui_text_entry_new(ui_font_t *font) {
    ui_text_entry_t *entry = calloc(1, sizeof(ui_text_entry_t));

    entry->element.draw = &ui_text_entry_draw;
    entry->element.get_preferred_size = &ui_text_entry_get_preferred_size;
    entry->element.free = &ui_text_entry_free;
    entry->element.process_mouse_event = &ui_text_entry_process_mouse_event;
    entry->element.process_keyboard_event = &ui_text_entry_process_keyboard_event;

    entry->font = font;
    entry->text = calloc(MAX_TEXT_LEN + 1, sizeof(char));

    entry->pref_width = 50;
    entry->pref_height = ui_font_get_line_spacing(entry->font) + 4;

    ui_element_ref(entry);

    return entry;
}

void ui_text_entry_free(ui_text_entry_t *entry) {
    if (entry->lua_cbi) lua_manager_unref(entry->lua_cbi);
    free(entry->text);
    free(entry);
}

const char *ui_text_entry_get_text(ui_text_entry_t *entry) {
    return entry->text;
}

void ui_text_entry_set_text(ui_text_entry_t *entry, const char *text) {
    memset(entry->text, 0, MAX_TEXT_LEN);
    memcpy(entry->text, text, strlen(text));

    entry->pref_width = ui_font_get_text_width(entry->font, entry->text, (int)strlen(entry->text));
}

int ui_text_lua_keydown_event_run_callback(lua_State *L, keydown_event_data_t *data) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, data->entry->lua_cbi);
    lua_pushstring(L, data->key_name);

    free(data->key_name);
    free(data);

    return 1;
}

int ui_text_entry_lua_new(lua_State *L);
int ui_text_entry_lua_del(lua_State *L);
int ui_text_entry_lua_hint(lua_State *L);
int ui_text_entry_lua_text(lua_State *L);
int ui_text_entry_lua_on_keydown(lua_State *L);

void ui_text_entry_lua_register_funcs(lua_State *L) {
    lua_pushcfunction(L, &ui_text_entry_lua_new);
    lua_setfield(L, -2, "text_entry");
}

int ui_text_entry_get_preferred_size(ui_text_entry_t *entry, int *width, int *height) {
    *width = entry->pref_width;
    *height = entry->pref_height;

    return 1;
}

void ui_text_entry_draw(ui_text_entry_t *entry, int offset_x, int offset_y, mat4f_t *proj) {
    ui_color_t bg_color = 0x262626FF;
    ui_color_t border_color = 0;
    ui_color_t border_highlight_color = 0;
    ui_color_t text_color = 0;

    ui_color_t hint_color = 0x707070FF;

    int have_focus = ui_element_has_focus(entry);

    //GET_APP_SETTING_INT("overlay.ui.colors.windowBG",              (int*)&bg_color);
    GET_APP_SETTING_INT("overlay.ui.colors.windowBorder",          (int*)&border_color);
    GET_APP_SETTING_INT("overlay.ui.colors.windowBorderHighlight", (int*)&border_highlight_color);
    GET_APP_SETTING_INT("overlay.ui.colors.text",                  (int*)&text_color);

    int ex = offset_x + entry->element.x;
    int ey = offset_y + entry->element.y;

    // draw background
    ui_rect_draw(ex, ey, entry->element.width, entry->element.height, bg_color, proj);

    // draw borders
    ui_color_t bc = have_focus ? border_highlight_color : border_color;
    ui_rect_draw(ex, ey, entry->element.width, 1, bc, proj);                             // top
    ui_rect_draw(ex, ey + entry->element.height - 1, entry->element.width, 1, bc, proj); // bottom
    ui_rect_draw(ex, ey, 1, entry->element.height, bc, proj);                            // left
    ui_rect_draw(ex + entry->element.width - 1, ey, 1, entry->element.height, bc, proj); // right

    // hint_text
    if (strlen(entry->text)==0 && entry->hint) {
        int old_scissor[4];
        if (push_scissor(ex+1, ey+1, entry->element.width-2, entry->element.height-2, old_scissor)) {
            ui_font_render_text(entry->font, proj, ex+2, ey+2, entry->hint, strlen(entry->hint), hint_color);

            pop_scissor(old_scissor);
        }
    } else if (strlen(entry->text)) {
        int old_scissor[4];
        if (push_scissor(ex+1, ey+1, entry->element.width-2, entry->element.height-2, old_scissor)) {
            ui_font_render_text(entry->font, proj, ex+2, ey+2, entry->text, entry->text_len, text_color);

            pop_scissor(old_scissor);
        }
    }

    if (have_focus) {        
        if (entry->caret_blink) {
            ui_rect_draw(ex + 2 + entry->caret_x, ey + 2, 1, entry->element.height-4, text_color, proj);
        }

        double now = glfwGetTime();
        if (now - entry->caret_counter > 0.5) {
            entry->caret_blink = entry->caret_blink ? 0 : 1;
            entry->caret_counter = now;
        }
    }

    ui_add_input_element(offset_x, offset_y, entry->element.x, entry->element.y, entry->element.width, entry->element.height, (ui_element_t*)entry);
}

int ui_text_entry_process_mouse_event(ui_text_entry_t *entry, ui_mouse_event_t *event, int offset_x, int offset_y) {
    if (ui_element_has_focus(entry)) {
        if (event->event==UI_MOUSE_EVENT_TYPE_BTN_UP && event->button==UI_MOUSE_EVENT_BUTTON_LEFT) {
            int text_x = event->x - offset_x - entry->element.x - 2;
            int text_y = event->y - offset_y - entry->element.y - 2;
            if (text_x < 0 || text_x > entry->element.width - 4) return 0;
            if (text_y < 0 || text_y > entry->element.height - 4) return 0;

            entry->caret_pos = ui_font_get_index_of_width(entry->font, entry->text, text_x);
            entry->caret_x = ui_font_get_text_width(entry->font, entry->text, entry->caret_pos);

            return 1;
        }
    } else {
        if (event->event==UI_MOUSE_EVENT_TYPE_BTN_UP && event->button==UI_MOUSE_EVENT_BUTTON_LEFT) {
            ui_grab_focus(entry);

            return 1;
        }
    }

    return 1;
}

void ui_text_entry_set_caret_pos(ui_text_entry_t *entry, int caret_pos) {
    entry->caret_pos = caret_pos;
    entry->caret_x = ui_font_get_text_width(entry->font, entry->text, entry->caret_pos);
}

int ui_text_entry_process_keyboard_event(ui_text_entry_t *entry, ui_keyboard_event_t *event) {
    //logger_t *log = logger_get("ui-text-entry");


    if (event->vk_key==VK_SHIFT ||
        event->vk_key==VK_LSHIFT ||
        event->vk_key==VK_RSHIFT ||
        event->vk_key==VK_LMENU ||
        event->vk_key==VK_RMENU ||
        event->vk_key==VK_MENU ||
        event->vk_key==VK_CONTROL ||
        event->vk_key==VK_LCONTROL ||
        event->vk_key==VK_RCONTROL ||
        event->vk_key==VK_CAPITAL) return 0;

    
    if (!event->down) return 1;

    if (event->vk_key==VK_BACK) {
        if (event->down && entry->caret_pos) {
            memcpy(entry->text + entry->caret_pos - 1, entry->text + entry->caret_pos, entry->text_len - entry->caret_pos + 1);
            entry->text_len--;
            ui_text_entry_set_caret_pos(entry, entry->caret_pos - 1);
        }
        return 1;
    }

    if (event->vk_key==VK_DELETE) {
        if (event->down && entry->caret_pos < entry->text_len) {
            memcpy(entry->text + entry->caret_pos, entry->text + entry->caret_pos + 1, entry->text_len - entry->caret_pos);
            entry->text_len--;
        }
        return 1;
    }

    if (event->vk_key==VK_RETURN || event->vk_key==VK_UP || event->vk_key==VK_DOWN) {
        keydown_event_data_t *kdevent = calloc(1, sizeof(keydown_event_data_t));
        kdevent->entry = entry;
        
        if (event->vk_key==VK_RETURN) {
            kdevent->key_name = calloc(strlen("return")+1, sizeof(char));
            memcpy(kdevent->key_name, "return", strlen("return"));
        } else if (event->vk_key==VK_UP) {
            kdevent->key_name = calloc(strlen("up")+1, sizeof(char));
            memcpy(kdevent->key_name, "up", strlen("up"));
        } else if (event->vk_key==VK_DOWN) {
            kdevent->key_name = calloc(strlen("down")+1, sizeof(char));
            memcpy(kdevent->key_name, "down", strlen("down"));
        }

        if (entry->lua_cbi) lua_manager_add_event_callback(&ui_text_lua_keydown_event_run_callback, kdevent);
        if (event->vk_key==VK_RETURN) return 1;
    }


    if (event->vk_key==VK_LEFT || event->vk_key==VK_RIGHT) {
        int newpos = entry->caret_pos + (event->vk_key==VK_LEFT ? -1 : 1);
        if (newpos<0) newpos = 0;
        if (newpos>entry->text_len) newpos = (int)entry->text_len;
        ui_text_entry_set_caret_pos(entry, newpos);
        return 1;
    }

    if (event->vk_key==VK_END) {
        ui_text_entry_set_caret_pos(entry, (int)entry->text_len);
        return 1;
    }

    if (event->vk_key==VK_HOME) {
        ui_text_entry_set_caret_pos(entry, 0);
        return 1;
    }

    if (event->ctrl && !event->alt && !event->shift && event->vk_key=='V') {
        char *clipboard_text = app_getclipboard_text();
        if (clipboard_text) {
            size_t ct_len = strlen(clipboard_text);
            if (entry->text_len + ct_len < MAX_TEXT_LEN) {
                if (entry->caret_pos < entry->text_len) {
                    char *text_right = calloc(entry->text_len - entry->caret_pos, sizeof(char));
                    memcpy(text_right, entry->text + entry->caret_pos, entry->text_len - entry->caret_pos);

                    memcpy(entry->text + entry->caret_pos + strlen(clipboard_text), text_right, entry->text_len - entry->caret_pos);
                    free(text_right);
                }
            }

            memcpy(entry->text + entry->caret_pos, clipboard_text, strlen(clipboard_text));
            entry->text_len += strlen(clipboard_text);
            ui_text_entry_set_caret_pos(entry, entry->caret_pos + (int)strlen(clipboard_text));
        }
        free(clipboard_text);
    }

    if (!event->alt && !event->ctrl && event->ascii[0] && entry->text_len < MAX_TEXT_LEN) {
        int len = 1;
        if (event->ascii[1]) len++;

        if (entry->caret_pos < entry->text_len) {
            char *text_right = calloc(entry->text_len - entry->caret_pos, sizeof(char));
            memcpy(text_right, entry->text + entry->caret_pos, entry->text_len - entry->caret_pos);

            memcpy(entry->text + entry->caret_pos + len, text_right, entry->text_len - entry->caret_pos);
            free(text_right);
        }        

        entry->text[entry->caret_pos++] = event->ascii[0];
        entry->text_len++;

        if (event->ascii[1]) {
            entry->text[entry->caret_pos++] = event->ascii[1];
            entry->text_len++;
        }

        entry->caret_x = ui_font_get_text_width(entry->font, entry->text, entry->caret_pos);

        keydown_event_data_t *kdevent = calloc(1, sizeof(keydown_event_data_t));
        kdevent->entry = entry;
        kdevent->key_name = calloc(len+1, sizeof(char));
        memcpy(kdevent->key_name, event->ascii, len);

        if (entry->lua_cbi) lua_manager_add_event_callback(&ui_text_lua_keydown_event_run_callback, kdevent);
    }

    return 1;
}

luaL_Reg ui_text_entry_funcs[] = {
    "__gc",       &ui_text_entry_lua_del,
    "hint",       &ui_text_entry_lua_hint,
    "text",       &ui_text_entry_lua_text,
    "on_keydown", &ui_text_entry_lua_on_keydown,
    NULL,         NULL
};

void ui_text_entry_lua_register_metatable(lua_State *L) {
    if (luaL_newmetatable(L, "UITextEntryMetaTable")) {
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");

        luaL_setfuncs(L, ui_text_entry_funcs, 0);
    }
}

void lua_push_ui_textentry(lua_State *L, ui_text_entry_t *entry) {
    ui_element_ref(entry);

    ui_text_entry_t **pentry = (ui_text_entry_t**)lua_newuserdata(L, sizeof(ui_text_entry_t*));
    *pentry = entry;

    ui_text_entry_lua_register_metatable(L);
    lua_setmetatable(L, -2);
}

/*** RST
Text Entry Elements
===================

.. lua:currentmodule:: eg-overlay-ui

Functions
---------

.. lua:function:: text_entry(fontname, fontsize, fontweight, fontslant, fontwidth)

    Create a new :lua:class:`uitextentry`.

    .. note::
        See :ref:`fonts` for more detail on the font* parameters.

    :param fontpath: The path to the font to use.
    :type fontpath: string
    :param fontsize: The font size, expressed as a pixel height.
    :type fontsize: integer
    :param fontweight: (Optional) The font weight.
    :type fontweight: integer
    :param fontslant: (Optional) The font slant.
    :type fontslant: integer
    :param fontwidth: (Optional) the font width.
    :type fontwidth: integer
    :rtype: uitextentry

    .. versionhistory::
        :0.0.1: Added
*/

int ui_text_entry_lua_new(lua_State *L) {
    const char *font_name = luaL_checkstring(L, 1);
    int font_size = (int)luaL_checkinteger(L, 2);
    int weight = INT_MIN;
    int slant = INT_MIN;
    int width = INT_MIN;

    if (lua_gettop(L)>=3) weight = (int)luaL_checkinteger(L, 3);
    if (lua_gettop(L)>=4) slant  = (int)luaL_checkinteger(L, 4);
    if (lua_gettop(L)>=5) width  = (int)luaL_checkinteger(L, 5);

    ui_font_t *font = ui_font_get(font_name, font_size, weight, slant, width);

    ui_text_entry_t *entry = ui_text_entry_new(font);

    lua_push_ui_textentry(L, entry);

    ui_element_unref(entry);

    return 1;
}

/*** RST
Classes
-------

.. lua:class:: uitextentry

    A text box that accepts a single line of text.

*/

#define LUA_CHECK_TEXT_ENTRY(L, ind) *(ui_text_entry_t**)luaL_checkudata(L, ind, "UITextEntryMetaTable")

int ui_text_entry_lua_del(lua_State *L) {
    ui_text_entry_t *entry = LUA_CHECK_TEXT_ENTRY(L, 1);

    ui_element_unref(entry);

    return 0;
}

/*** RST
    .. lua:method:: hint(text)

        Set a hint that will be shown whenever the text entry is empty.

        :param text:
        :type text: string

        .. versionhistory::
            :0.0.1: Added
*/
int ui_text_entry_lua_hint(lua_State *L) {
    ui_text_entry_t *entry = LUA_CHECK_TEXT_ENTRY(L, 1);
    const char *hint = luaL_checkstring(L, 2);

    if (entry->hint) free(entry->hint);
    size_t hint_len = strlen(hint);
    if (strlen(hint)<1) return 0;

    entry->hint = calloc(hint_len + 1, sizeof(char));
    memcpy(entry->hint, hint, hint_len);

    return 0;
}

/*** RST
    .. lua:method:: text(value)

        Get or set the current text.

        :parameter string value:
        :return: The current text is returned if ``value`` is omitted otherwise
            ``nil``
        :rtype: string

        .. versionhistory::
            :0.0.1: Added
*/
int ui_text_entry_lua_text(lua_State *L) {
    ui_text_entry_t *entry = LUA_CHECK_TEXT_ENTRY(L, 1);

    if (lua_gettop(L)==1) {
        lua_pushstring(L, entry->text);
        return 1;
    } else if (lua_gettop(L)==2) {
        const char *text = luaL_checkstring(L, 2);
        entry->text_len = strlen(text);
        memset(entry->text, 0, MAX_TEXT_LEN);
        memcpy(entry->text, text, entry->text_len);

        entry->caret_pos = (int)entry->text_len;
        entry->caret_x = ui_font_get_text_width(entry->font, entry->text, (int)strlen(entry->text));

        return 0;
    }

    return luaL_error(L, "entry_text:text takes either 0 or 1 argument.");
}

/*** RST
    .. lua:method:: on_keydown([handler])

        Set or clear an event handler to be called on every key press. Only one
        handler can be set at a time.

        .. important::
            ``handler`` is called with a single string argument that is either
            the ascii value of the key press, ie. ``'a'`` or ``'A'``, or the
            following special keys: ``'return'``, ``'up'``, ``'down'``.

        :param function handler:

        .. versionhistory::
            :0.0.1: Added
*/
int ui_text_entry_lua_on_keydown(lua_State *L) {
    ui_text_entry_t *entry = LUA_CHECK_TEXT_ENTRY(L, 1);

    if (lua_gettop(L)==1) {
        luaL_unref(L, LUA_REGISTRYINDEX, entry->lua_cbi);
        entry->lua_cbi = 0;
        return 0;
    }

    if (lua_gettop(L)!=2 || !lua_isfunction(L, 2)) {
        return luaL_error(L, "text_entry:on_keydown accepts either no argument or a function.");
    }


    if (entry->lua_cbi) luaL_unref(L, LUA_REGISTRYINDEX, entry->lua_cbi);

    lua_pushvalue(L, 2);
    entry->lua_cbi = luaL_ref(L, LUA_REGISTRYINDEX);

    return 0;
}
