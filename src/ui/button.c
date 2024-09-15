#include "button.h"
#include "ui.h"
#include "utils.h"
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

    int event_cbi;

    int draw_state;
    int state; // checkbox/toggle
    int pref_size; // checkbox

    int lua_bind_table;
    char *lua_bind_field;
};

typedef enum {
    UI_BUTTON_EVENT_TYPE_ENTER,
    UI_BUTTON_EVENT_TYPE_LEAVE,
    UI_BUTTON_EVENT_TYPE_MOUSE_DOWN_LEFT,
    UI_BUTTON_EVENT_TYPE_MOUSE_DOWN_RIGHT,
    UI_BUTTON_EVENT_TYPE_MOUSE_UP_LEFT,
    UI_BUTTON_EVENT_TYPE_MOUSE_UP_RIGHT,
    UI_BUTTON_EVENT_TYPE_MOUSE_CLICK_LEFT,
    UI_BUTTON_EVENT_TYPE_MOUSE_CLICK_RIGHT,
    UI_BUTTON_EVENT_TYPE_TOGGLE_ON,
    UI_BUTTON_EVENT_TYPE_TOGGLE_OFF
} ui_button_event_type;

typedef struct ui_button_lua_event_data_t {
    ui_button_t *button;
    ui_button_event_type event;
} ui_button_lua_event_data_t;

int ui_button_lua_event_callback(lua_State *L, ui_button_lua_event_data_t *data);

void ui_button_free(ui_button_t *button);
void ui_button_draw(ui_button_t *button, int offset_x, int offset_y, mat4f_t *proj);
int ui_button_get_preferred_size(ui_button_t *button, int *width, int *height);
int ui_button_process_mouse_event(ui_button_t *button, ui_mouse_event_t *event, int offset_x, int offset_y);

ui_button_t *ui_button_new() {
    ui_button_t *btn = calloc(1, sizeof(ui_button_t));

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
    if (button->event_cbi) lua_manager_unref(button->event_cbi);
    if (button->child) ui_element_unref(button->child);
    if (button->lua_bind_table) {
        lua_manager_unref(button->lua_bind_table);
        free(button->lua_bind_field);
    }
    free(button);
}

void ui_button_draw(ui_button_t *button, int offset_x, int offset_y, mat4f_t *proj) {
    int cw = button->element.width - (button->border_width * 2);;
    int ch = ch = button->element.height - (button->border_width * 2);
    //if (!button->child || !button->child->get_preferred_size || !button->child->get_preferred_size(button->child, &cw, &ch)) return;

    //if (cw > button->element.width - (button->border_width * 2)) cw = button->element.width - (button->border_width * 2);
    //if (ch > button->element.height - (button->border_width * 2)) ch = button->element.height - (button->border_width * 2);

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

    // background
    ui_rect_draw(
        offset_x + button->element.x + button->border_width,
        offset_y + button->element.y + button->border_width,
        button_width - (button->border_width * 2),
        button_height - (button->border_width * 2),
        button->bg_highlight ? button->background_highlight : (button->bg_hover ? button->background_hover : button->background),
        proj
    );

    if (button->lua_bind_table && button->draw_state) button->state = lua_manager_gettableref_bool(button->lua_bind_table, button->lua_bind_field);

    if (button->draw_state && button->state) ui_rect_draw(offset_x + button->element.x + button->border_width, offset_y + button->element.y + button->border_width, button->element.width - (button->border_width * 2), button->element.height - (button->border_width * 2), button->state_color, proj);

    if (button->child) ui_element_draw(button->child, offset_x + button->element.x + button->border_width, offset_y + button->element.y + button->border_width, proj);
    //if (button->child && button->child->draw) button->child->draw(button->child, offset_x + button->element.x + button->border_width, offset_y + button->element.y + button->border_width, proj);

    ui_add_input_element(offset_x, offset_y, button->element.x, button->element.y, button->element.width, button->element.height, (ui_element_t*)button);
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

ui_button_lua_event_data_t *new_event_data(ui_button_t *button, ui_button_event_type type) {
    ui_button_lua_event_data_t *data = calloc(1, sizeof(ui_button_lua_event_data_t));
    data->button = button;
    data->event = type;
    
    return data;
}

int ui_button_process_mouse_event(ui_button_t *button, ui_mouse_event_t *event, int offset_x, int offset_y) {

    if (event->event==UI_MOUSE_EVENT_TYPE_ENTER) {
        ui_button_lua_event_data_t *data = new_event_data(button, UI_BUTTON_EVENT_TYPE_ENTER);
        if (button->event_cbi) lua_manager_add_event_callback(&ui_button_lua_event_callback, data);
        button->bg_hover = 1;
    } else if (event->event==UI_MOUSE_EVENT_TYPE_LEAVE) {
        ui_button_lua_event_data_t *data = new_event_data(button, UI_BUTTON_EVENT_TYPE_LEAVE);
        if (button->event_cbi) lua_manager_add_event_callback(&ui_button_lua_event_callback, data);
        button->bg_hover = 0;
    } else if (event->event==UI_MOUSE_EVENT_TYPE_BTN_DOWN) {
        ui_capture_mouse_events((ui_element_t*)button, offset_x, offset_y);
        ui_button_lua_event_data_t *data = NULL;
        if (event->button==UI_MOUSE_EVENT_BUTTON_LEFT) data = new_event_data(button, UI_BUTTON_EVENT_TYPE_MOUSE_DOWN_LEFT);
        else if (event->button==UI_MOUSE_EVENT_BUTTON_RIGHT) data = new_event_data(button, UI_BUTTON_EVENT_TYPE_MOUSE_DOWN_RIGHT);
        if (data && button->event_cbi) lua_manager_add_event_callback(&ui_button_lua_event_callback, data);
        button->bg_highlight = 1;
    } else if (event->event==UI_MOUSE_EVENT_TYPE_BTN_UP && button->bg_highlight==1) {
        ui_capture_mouse_events((ui_element_t*)button, offset_x, offset_y);
        ui_button_lua_event_data_t *data = NULL;
        if (event->button==UI_MOUSE_EVENT_BUTTON_LEFT) data = new_event_data(button, UI_BUTTON_EVENT_TYPE_MOUSE_UP_LEFT);
        else if (event->button==UI_MOUSE_EVENT_BUTTON_RIGHT) data = new_event_data(button, UI_BUTTON_EVENT_TYPE_MOUSE_UP_RIGHT);
        if (data && button->event_cbi) lua_manager_add_event_callback(&ui_button_lua_event_callback, data);

        if (MOUSE_POINT_IN_RECT(event->x, event->y, offset_x + button->element.x, offset_y + button->element.y, button->element.width, button->element.height)) {
            // the up happned while still over the button, this is a 'click'
            ui_button_lua_event_data_t *clickdata = NULL;
            if (event->button==UI_MOUSE_EVENT_BUTTON_LEFT) clickdata = new_event_data(button, UI_BUTTON_EVENT_TYPE_MOUSE_CLICK_LEFT);
            else if (event->button==UI_MOUSE_EVENT_BUTTON_RIGHT) clickdata = new_event_data(button, UI_BUTTON_EVENT_TYPE_MOUSE_CLICK_RIGHT);
            if (clickdata && button->event_cbi) lua_manager_add_event_callback(&ui_button_lua_event_callback, clickdata);
            button->state = button->state ? 0 : 1;
            if (button->lua_bind_table) {
                lua_manager_settabletref_bool(button->lua_bind_table, button->lua_bind_field, button->state);
            }
            if (button->draw_state) {
                // this is a toggle/checkbox. send a toggle-on or toggle-off event
                ui_button_lua_event_data_t *toggledata = NULL;
                if (button->state) toggledata = new_event_data(button, UI_BUTTON_EVENT_TYPE_TOGGLE_ON);
                else               toggledata = new_event_data(button, UI_BUTTON_EVENT_TYPE_TOGGLE_OFF);
                if (button->event_cbi) lua_manager_add_event_callback(&ui_button_lua_event_callback, toggledata);
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

int ui_button_lua_event_callback(lua_State *L, ui_button_lua_event_data_t *data) {

    lua_rawgeti(L, LUA_REGISTRYINDEX, data->button->event_cbi);

    switch (data->event) {
    case UI_BUTTON_EVENT_TYPE_ENTER:             lua_pushliteral(L, "enter"      ); break;
    case UI_BUTTON_EVENT_TYPE_LEAVE:             lua_pushliteral(L, "leave"      ); break;
    case UI_BUTTON_EVENT_TYPE_MOUSE_CLICK_LEFT:  lua_pushliteral(L, "click-left" ); break;
    case UI_BUTTON_EVENT_TYPE_MOUSE_CLICK_RIGHT: lua_pushliteral(L, "click-right"); break;
    case UI_BUTTON_EVENT_TYPE_MOUSE_DOWN_LEFT:   lua_pushliteral(L, "down-left"  ); break;
    case UI_BUTTON_EVENT_TYPE_MOUSE_DOWN_RIGHT:  lua_pushliteral(L, "down-right" ); break;
    case UI_BUTTON_EVENT_TYPE_MOUSE_UP_LEFT:     lua_pushliteral(L, "up-left"    ); break;
    case UI_BUTTON_EVENT_TYPE_MOUSE_UP_RIGHT:    lua_pushliteral(L, "up-right"   ); break;
    case UI_BUTTON_EVENT_TYPE_TOGGLE_ON:         lua_pushliteral(L, "toggle-on"  ); break;
    case UI_BUTTON_EVENT_TYPE_TOGGLE_OFF:        lua_pushliteral(L, "toggle-off" ); break;
    default:                                     lua_pushliteral(L, "unknown"    ); break;
    }

    /*
    if (lua_pcall(L, 1, 0, 0)!=LUA_OK) {
        const char *err = lua_tostring(L, -1);
        logger_t *log = logger_get("ui-button");
        logger_error(log, "Error while running button event callback: %s", err);
        lua_pop(L, 1);
    }
    */

    free(data);

    return 1;
}

int ui_button_lua_new(lua_State *L);
int ui_checkbox_lua_new(lua_State *L);

int ui_button_lua_del(lua_State *L);
int ui_button_lua_set_child(lua_State *L);
int ui_button_lua_background_color(lua_State *L);
int ui_button_lua_border_color(lua_State *L);
int ui_button_lua_border_width(lua_State *L);
int ui_button_lua_event_handler(lua_State *L);
int ui_button_lua_bind_value(lua_State *L);

void lua_push_ui_button(lua_State *L, ui_button_t *button);

luaL_Reg ui_mod_button_funcs[] = {
    "button"  , &ui_button_lua_new,
    "checkbox", &ui_checkbox_lua_new,
    NULL      ,  NULL
};

void ui_button_lua_register_ui_funcs(lua_State *L) {
    luaL_setfuncs(L, ui_mod_button_funcs, 0);
}

/*
static int ui_image_button_process_mouse_event(ui_image_button_t *btn, ui_mouse_event_t *me, int offset_x, int offset_y) {
    if (me->event==UI_MOUSE_EVENT_TYPE_ENTER) {
        btn->mouse_over = 1;
        btn->img_sat = 1.f;
        return 0;
    } else if (me->event==UI_MOUSE_EVENT_TYPE_LEAVE) {
        btn->mouse_over = 0;
        btn->img_sat = 0.f;
        return 0;
    }
    
    if (MOUSE_EVENT_IS_LEFT_DN(me)) {
        btn->img_val = 1.5f;
        btn->mouse_down = 1;
        ui_capture_mouse_events((ui_element_t*)btn, offset_x, offset_y);
    }
    if (MOUSE_EVENT_IS_LEFT_UP(me) && btn->mouse_down) {
        btn->mouse_down = 0;
        if (btn->mouse_over) {

            // first call callbacks here in C
            ui_button_event_callback_list_t *c_cb = btn->event_callbacks;
            while (c_cb) {
                c_cb->callback(UI_BUTTON_EVENT_TYPE_LEFT_CLICK);
                c_cb = c_cb->next;
            }

            // then Lua callbacks
            ui_button_lua_event_callback_list_t *l_cb = btn->lua_event_callbacks;
            while (l_cb) {
                lua_manager_add_event_callback(&lua_event_run_callback, l_cb);
                l_cb = l_cb->next;
            }
        }
        btn->img_val = 1.f;
        ui_release_mouse_events();            
    }

    return 1;
}
*/

luaL_Reg ui_button_funcs[] = {
    "__gc"            , &ui_button_lua_del,
    "set_child"       , &ui_button_lua_set_child,
    "background_color", &ui_button_lua_background_color,
    "border_color"    , &ui_button_lua_border_color,
    "border_width"    , &ui_button_lua_border_width,
    "event_handler"   , &ui_button_lua_event_handler,
    "bind_value"      , &ui_button_lua_bind_value,
    NULL              ,  NULL
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
    lua_push_ui_button(L, btn);
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
    lua_push_ui_button(L, checkbox);
    ui_element_unref(checkbox);

    return 1;
}

#define LUA_CHECK_BUTTON(L, ind) *(ui_button_t**)luaL_checkudata(L, ind, "UIButtonMetaTable")

int ui_button_lua_del(lua_State *L) {
    ui_button_t *btn = LUA_CHECK_BUTTON(L, 1);

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

    .. lua:method:: set_child(uielement)

        Set the child element of this button.

        :param uielement: A UI element.

        .. versionhistory::
            :0.0.1: Added
*/
int ui_button_lua_set_child(lua_State *L) {
    ui_button_t *btn = LUA_CHECK_BUTTON(L, 1);

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
    ui_button_t *btn = LUA_CHECK_BUTTON(L, 1);
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
    ui_button_t *btn = LUA_CHECK_BUTTON(L, 1);
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
    ui_button_t *btn = LUA_CHECK_BUTTON(L, 1);
    int width = (int)luaL_checkinteger(L, 2);

    btn->border_width = width;

    return 0;
}

/*** RST
    .. lua:method:: event_handler(handler_func)

        Set a function to be called on button events. A button can only have a
        single event handler at a time.

        The handler is called with a single string argument that indicates the
        type of event:

        =========== =====================================
        Value       Description
        =========== =====================================
        enter       Mouse cursor entered the button.
        leave       Mouse cursor left the button.
        click-left  Left click (sent on button release).
        click-right Right click (sent on button release).
        down-left   Left mouse button down.
        down-right  Right mouse button down.
        up-left     Left mouse button released.
        up-right    Right mouse button released.
        toggle-on   Button toggled on or checked.
        toggle-off  Button toggled off or un-checked.
        =========== =====================================

        :param handler_func: A function to be called on button events.
        :type handler_func: function

        .. versionhistory::
            :0.0.1: Added
*/
int ui_button_lua_event_handler(lua_State *L) {
    ui_button_t *btn = LUA_CHECK_BUTTON(L, 1);

    if (!lua_isfunction(L, 2)) return luaL_error(L, "button:event_handler argument #1 must be a function.");

    if (btn->event_cbi) luaL_unref(L, LUA_REGISTRYINDEX, btn->event_cbi);
    lua_pushvalue(L, 2);
    btn->event_cbi = luaL_ref(L, LUA_REGISTRYINDEX);

    return 0;
}

void lua_push_ui_button(lua_State *L, ui_button_t *button) {
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
    ui_button_t *btn = LUA_CHECK_BUTTON(L, 1);

    if (lua_gettop(L)!=3) return luaL_error(L, "button:bind_value takes 2 arguments: table, fieldname.");

    if (!lua_istable(L, 2)) return luaL_error(L, "button:bind_value argument #1 must be a table.");
    if (!lua_isstring(L, 3)) return luaL_error(L, "button:bind_value argument #2 must be a string.");
    
    if (btn->lua_bind_table) {
        luaL_unref(L, LUA_REGISTRYINDEX, btn->lua_bind_table);
        free(btn->lua_bind_field);
    }

    const char *field = lua_tostring(L, 3);

    lua_pushvalue(L, 2);
    btn->lua_bind_table = luaL_ref(L, LUA_REGISTRYINDEX);

    btn->lua_bind_field = calloc(strlen(field)+1, sizeof(char));
    memcpy(btn->lua_bind_field, field, strlen(field));

    return 0;
}
