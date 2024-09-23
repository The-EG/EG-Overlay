#include "button.h"
#include "ui.h"
#include "../utils.h"
#include "rect.h"
#include "text.h"
#include "../app.h"
#include "../logging/logger.h"
#include "../lua-manager.h"
#include <lauxlib.h>

struct ui_button_t {
    ui_element_t element;

    ui_color_t background;
    ui_color_t background_hover;
    ui_color_t background_highlight;
    ui_color_t border_color;
    ui_color_t state_color;

    int bg_hover;
    int bg_highlight;

    int border_width;

    ui_element_t *child;

    int draw_state;
    int state; // checkbox/toggle
    int pref_size; // checkbox

    int lua_bind_table;
    char *lua_bind_field;
};


void ui_button_free(ui_button_t *button);
void ui_button_draw(ui_button_t *button, int offset_x, int offset_y, mat4f_t *proj);
int ui_button_get_preferred_size(ui_button_t *button, int *width, int *height);
int ui_button_process_mouse_event(ui_button_t *button, ui_mouse_event_t *event, int offset_x, int offset_y);

ui_button_t *ui_button_new() {
    ui_button_t *btn = egoverlay_calloc(1, sizeof(ui_button_t));

    btn->element.draw                = &ui_button_draw;
    btn->element.get_preferred_size  = &ui_button_get_preferred_size;
    btn->element.free                = &ui_button_free;
    btn->element.process_mouse_event = &ui_button_process_mouse_event;

    settings_t *app_settings = app_get_settings();
    settings_get_int(app_settings, "overlay.ui.colors.buttonBG"         , (int*)&btn->background);
    settings_get_int(app_settings, "overlay.ui.colors.buttonBGHover"    , (int*)&btn->background_hover);
    settings_get_int(app_settings, "overlay.ui.colors.buttonBGHighlight", (int*)&btn->background_highlight);
    settings_get_int(app_settings, "overlay.ui.colors.buttonBorder"     , (int*)&btn->border_color);
    btn->border_width = 1;

    ui_element_ref(btn);
    return btn;
}

ui_button_t *ui_checkbox_new(int size) {
    ui_button_t *cb = ui_button_new();

    cb->pref_size = size;
    cb->draw_state = 1;
    cb->border_width = 1;

    settings_t *app_settings = app_get_settings();
    settings_get_int(app_settings, "overlay.ui.colors.text", (int*)&cb->state_color);

    return cb;
}

void ui_button_free(ui_button_t *button) {
    if (button->child) ui_element_unref(button->child);
    if (button->lua_bind_table) {
        lua_manager_unref(button->lua_bind_table);
        egoverlay_free(button->lua_bind_field);
    }
    egoverlay_free(button);
}

void ui_button_draw(ui_button_t *button, int offset_x, int offset_y, mat4f_t *proj) {
    int cw = button->element.width - (button->border_width * 2);;
    int ch = ch = button->element.height - (button->border_width * 2);
    if (button->child) button->child->width = cw;
    if (button->child) button->child->height = ch;

    int button_width = button->element.width;
    int button_height = button->element.height;

    if (button->border_width) {
        ui_rect_draw(  // top
            offset_x + button->element.x,
            offset_y + button->element.y,
            button_width,
            button->border_width,
            button->border_color,
            proj
        );
        ui_rect_draw( // bottom
            offset_x + button->element.x,
            offset_y + button->element.y + button_height - button->border_width,
            button_width,
            button->border_width,
            button->border_color,
            proj
        );
        ui_rect_draw( // left
            offset_x + button->element.x,
            offset_y + button->element.y + button->border_width,
            button->border_width, button_height - (button->border_width * 2),
            button->border_color,
            proj
        );
        ui_rect_draw( // right
            offset_x + button->element.x + button_width - button->border_width,
            offset_y + button->element.y + button->border_width,
            button->border_width,
            button_height - (button->border_width * 2),
            button->border_color,
            proj
        );
    }

    ui_color_t bg = button->background;

    if (button->bg_highlight) {
        bg = button->background_highlight;
    } else if (button->bg_hover) {
        bg = button->background_hover;
    }

    // background
    ui_rect_draw(
        offset_x + button->element.x + button->border_width,
        offset_y + button->element.y + button->border_width,
        button_width - (button->border_width * 2),
        button_height - (button->border_width * 2),
        bg,
        proj
    );

    if (button->lua_bind_table && button->draw_state) {
        button->state = lua_manager_gettableref_bool(button->lua_bind_table, button->lua_bind_field);
    }

    if (button->draw_state && button->state) {
        ui_rect_draw(
            offset_x + button->element.x + button->border_width,
            offset_y + button->element.y + button->border_width,
            button->element.width - (button->border_width * 2),
            button->element.height - (button->border_width * 2),
            button->state_color,
            proj
        );
    }

    if (button->child) {
        ui_element_draw(
            button->child,
            offset_x + button->element.x + button->border_width,
            offset_y + button->element.y + button->border_width,
            proj
        );
    }

    ui_add_input_element(
        offset_x,
        offset_y,
        button->element.x,
        button->element.y,
        button->element.width,
        button->element.height,
        (ui_element_t*)button
    );
}

int ui_button_get_preferred_size(ui_button_t *button, int *width, int *height) {
    if (button->pref_size) {
        *width = button->pref_size;
        *height = button->pref_size;
        return 1;
    }

    if (button->child && button->child->get_preferred_size) {
        int w;
        int h;
        if (button->child->get_preferred_size(button->child, &w, &h)) {
            *width = w + (button->border_width * 2);
            *height = h + (button->border_width * 2);
            return 1;
        }
    }
    return 0;
}

int ui_button_process_mouse_event(ui_button_t *button, ui_mouse_event_t *event, int offset_x, int offset_y) {
    if (event->event==UI_MOUSE_EVENT_TYPE_ENTER) {
        button->bg_hover = 1;
    } else if (event->event==UI_MOUSE_EVENT_TYPE_LEAVE) {
        button->bg_hover = 0;
    } else if (event->event==UI_MOUSE_EVENT_TYPE_BTN_DOWN) {
        ui_capture_mouse_events((ui_element_t*)button, offset_x, offset_y);
        button->bg_highlight = 1;
    } else if (event->event==UI_MOUSE_EVENT_TYPE_BTN_UP && button->bg_highlight==1) {
        if (MOUSE_POINT_IN_RECT(
                event->x,
                event->y,
                offset_x + button->element.x,
                offset_y + button->element.y,
                button->element.width,
                button->element.height
        )) {
            // the up happened while still over the button, this is a 'click'
            if (event->button==UI_MOUSE_EVENT_BUTTON_LEFT) {
                ui_element_call_lua_event_handlers(button, "click-left");
            } else if (event->button==UI_MOUSE_EVENT_BUTTON_RIGHT) {
                ui_element_call_lua_event_handlers(button, "click-right");
            }

            button->state = button->state ? 0 : 1;
            if (button->lua_bind_table) {
                lua_manager_settabletref_bool(button->lua_bind_table, button->lua_bind_field, button->state);
            }
            if (button->draw_state) {
                // this is a toggle/checkbox. send a toggle-on or toggle-off event
                ui_element_call_lua_event_handlers(button, (button->state ? "toggle-on" : "toggle-off"));
            }
        }
        button->bg_highlight = 0;
        ui_release_mouse_events((ui_element_t*)button);
    }

    return 1;
}

void ui_button_set_child(ui_button_t *button, ui_element_t *child) {
    if (button->child) ui_element_unref(button->child);

    button->child = child;
    if (button->child) ui_element_ref(child);
}

int ui_button_lua_new(lua_State *L);
int ui_checkbox_lua_new(lua_State *L);

int ui_button_lua_del(lua_State *L);
int ui_button_lua_set_child(lua_State *L);
int ui_button_lua_background_color(lua_State *L);
int ui_button_lua_border_color(lua_State *L);
int ui_button_lua_border_width(lua_State *L);
int ui_button_lua_bind_value(lua_State *L);
int ui_button_lua_state(lua_State *L);

void lua_pushuibutton(lua_State *L, ui_button_t *button);
ui_button_t *lua_checkuibutton(lua_State *L, int ind);

luaL_Reg ui_mod_button_funcs[] = {
    "button"  , &ui_button_lua_new,
    "checkbox", &ui_checkbox_lua_new,
    NULL      ,  NULL
};

void ui_button_lua_register_ui_funcs(lua_State *L) {
    luaL_setfuncs(L, ui_mod_button_funcs, 0);
}

luaL_Reg ui_button_funcs[] = {
    "__gc"              , &ui_button_lua_del,
    "set_child"         , &ui_button_lua_set_child,
    "background_color"  , &ui_button_lua_background_color,
    "border_color"      , &ui_button_lua_border_color,
    "border_width"      , &ui_button_lua_border_width,
    "bind_value"        , &ui_button_lua_bind_value,
    "state"             , &ui_button_lua_state,
    "addeventhandler"   , &ui_element_lua_addeventhandler,
    "removeeventhandler", &ui_element_lua_removeeventhandler,
    NULL                ,  NULL
};

/*** RST
Buttons
=======

.. lua:currentmodule:: eg-overlay-ui

Functions
---------

.. lua:function:: button()

    Create a new :lua:class:`uibutton`.

    :rtype: uibutton

    .. versionhistory::
        :0.0.1: Added
*/
int ui_button_lua_new(lua_State *L) {
    ui_button_t *btn = ui_button_new();
    lua_pushuibutton(L, btn);
    ui_element_unref(btn);

    return 1;
}

/*** RST
.. lua:function:: checkbox(size)

    Create a new :lua:class:`uibutton` specialized as a checkbox.

    :param size: The size of the checkbox. This corresponds to a font height and
        should generally be 1.2 times the font height of surrounding text.
    :type size: integer

    :rtype: uibutton

    .. versionhistory::
        :0.0.1: Added
*/
int ui_checkbox_lua_new(lua_State *L) {
    int size = (int)luaL_checkinteger(L, 1);

    ui_button_t *checkbox = ui_checkbox_new(size);
    lua_pushuibutton(L, checkbox);
    ui_element_unref(checkbox);

    return 1;
}

ui_button_t *lua_checkuibutton(lua_State *L, int ind) {
    return *(ui_button_t**)luaL_checkudata(L, ind, "UIButtonMetaTable");
}

int ui_button_lua_del(lua_State *L) {
    ui_button_t *btn = lua_checkuibutton(L, 1);

    ui_element_unref(btn);

    return 0;
}

/*** RST
Classes
-------

.. lua:class:: uibutton

    A button element. Buttons do not display any content themselves outside of
    borders and background, instead they have a single child.
    
    This can be a :lua:class:`uitext` for a simple text button, but can also be
    a layout container that also has children for more complex buttons.

    In addition to the events in :ref:`ui-events`, buttons can send the
    following:

    =========== =============================================================
    Event       Description
    =========== =============================================================
    click-left  The button was clicked with the primary/left mouse button.
    click-right The button was clicked with the secondary/right mouse button.
    toggle-on   The button has changed to a toggled/checked state.
    toggle-off  The button has changed to a untoggled/unchecked state.
    =========== =============================================================

    .. important::

        The ``click-*`` events are only sent when the corresponding mouse button
        is both pressed and released while the cursor is over the button. This
        is a more accurate way of determining if the button was clicked
        intentionally than monitoring for only up or down events.

    .. lua:method:: set_child(uielement)

        Set the child element of this button.

        :param uielement: A UI element.

        .. versionhistory::
            :0.0.1: Added
*/
int ui_button_lua_set_child(lua_State *L) {
    ui_button_t *btn = lua_checkuibutton(L, 1);

    if (!lua_isuserdata(L, 2)) return luaL_error(L, "button:set_child argument #1 must be a UI element");

    ui_element_t *child = *(ui_element_t**)lua_touserdata(L, 2);

    ui_button_set_child(btn, child);

    return 0;
}

/*** RST
    .. lua:method:: background_color(color)
    
        Set the background color of this button.

        :param color: A new background color in RGBA format.
        :type color: integer

        .. versionhistory::
            :0.0.1: Added
*/
int ui_button_lua_background_color(lua_State *L) {
    ui_button_t *btn = lua_checkuibutton(L, 1);
    ui_color_t color = (ui_color_t)luaL_checkinteger(L, 2);

    btn->background = color;

    return 0;
}

/*** RST
    .. lua:method:: border_color(color)
    
        Set the border color of this button.

        :param color: A new border color in RGBA format.
        :type color: integer

        .. versionhistory::
            :0.0.1: Added
*/
int ui_button_lua_border_color(lua_State *L) {
    ui_button_t *btn = lua_checkuibutton(L, 1);
    ui_color_t color = (ui_color_t)luaL_checkinteger(L, 2);

    btn->border_color = color;

    return 0;
}

/*** RST
    .. lua:method:: border_width(width)
    
        Set the border width of this button.

        :param width: A new border width in pixels.
        :type color: integer

        .. versionhistory::
            :0.0.1: Added
*/
int ui_button_lua_border_width(lua_State *L) {
    ui_button_t *btn = lua_checkuibutton(L, 1);
    int width = (int)luaL_checkinteger(L, 2);

    btn->border_width = width;

    return 0;
}

void lua_pushuibutton(lua_State *L, ui_button_t *button) {
    ui_button_t **pbtn = (ui_button_t**)lua_newuserdata(L, sizeof(ui_button_t*));
    *pbtn = button;

    if (luaL_newmetatable(L, "UIButtonMetaTable")) {
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
    
        lua_pushboolean(L, 1);
        lua_setfield(L, -2, "__is_uielement");

        luaL_setfuncs(L, ui_button_funcs, 0);
    }

    lua_setmetatable(L, -2);
    ui_element_ref(button);
}

int ui_button_lua_bind_value(lua_State *L) {
    ui_button_t *btn = lua_checkuibutton(L, 1);

    if (lua_gettop(L)!=3) return luaL_error(L, "button:bind_value takes 2 arguments: table, fieldname.");

    if (!lua_istable(L, 2)) return luaL_error(L, "button:bind_value argument #1 must be a table.");
    if (!lua_isstring(L, 3)) return luaL_error(L, "button:bind_value argument #2 must be a string.");
    
    if (btn->lua_bind_table) {
        luaL_unref(L, LUA_REGISTRYINDEX, btn->lua_bind_table);
        egoverlay_free(btn->lua_bind_field);
    }

    const char *field = lua_tostring(L, 3);

    lua_pushvalue(L, 2);
    btn->lua_bind_table = luaL_ref(L, LUA_REGISTRYINDEX);

    btn->lua_bind_field = egoverlay_calloc(strlen(field)+1, sizeof(char));
    memcpy(btn->lua_bind_field, field, strlen(field));

    return 0;
}

/*** RST
    .. lua:method:: state([value])

        Get or return the state of this button.

        :param boolean value: (Optional)
        :rtype: boolean

        .. versionhistory::
            :0.1.0: Added

*/
int ui_button_lua_state(lua_State *L) {
    ui_button_t *btn = lua_checkuibutton(L, 1);

    if (lua_gettop(L)==2) {
        btn->state = lua_toboolean(L, 2);
        return 0;
    }
    lua_pushboolean(L, btn->state);
    return 1;
}

/*** RST
    .. include:: /docs/_include/ui_element_eventhandlers.rst
*/
