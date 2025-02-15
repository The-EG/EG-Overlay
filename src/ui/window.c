#include "window.h"
#include "font.h"
#include "text.h"
#include "rect.h"
#include "ui.h"
#include "../utils.h"
#include <string.h>
#include <lauxlib.h>
#include "../app.h"
#include "../logging/logger.h"
#include "../settings.h"
#include "../utils.h"
#include "../dx.h"

struct ui_window_s {
    ui_element_t element;

    char *caption;

    int moving;
    int sizing_h;
    int sizing_w;
    int move_last_x;
    int move_last_y;

    int highlight_title;
    int draw_resizer;

    int resizable;
    int resizing;

    int autosize;

    mat4f_t vp_proj;

    int child_x_offset;
    int child_y_offset;
    int child_width;
    int child_height;

    int vanchor;
    int hanchor;

    ui_element_t *child;

    settings_t *settings;
    char *settings_path;

    int showtitlebar;

    ui_color_t bg_color;
    ui_color_t border_color;
    ui_color_t border_hl_color;
    ui_color_t titlebar_text_color;

    int shown;
};

static ui_font_t *win_font = NULL;

void ui_window_free(ui_window_t *window);

static int ui_window_process_mouse_event(ui_window_t *win, ui_mouse_event_t *event, int offset_x, int offset_y);
void ui_window_draw(ui_window_t *win, int offset_x, int offset_y, mat4f_t *proj);

void ui_window_update_from_settings(ui_window_t *win);
void ui_window_save_to_settings(ui_window_t *win);

void ui_window_set_showtitlebar(ui_window_t *win, int showtitlebar);

static void ui_window_draw_decorations(ui_window_t *win, int offset_x, int offset_y, mat4f_t *proj) {
    int win_x = offset_x + win->element.x;
    int win_y = offset_y + win->element.y;

    // background
    ui_rect_draw(win_x, win_y, win->element.width, win->element.height, win->bg_color, proj);

    // borders
    ui_rect_draw(win_x, win_y, 1, win->element.height, win->border_color, proj); // left
    ui_rect_draw(win_x, win_y + win->element.height - 1, win->element.width, 1, win->border_color, proj); // bottom
    ui_rect_draw(win_x + win->element.width - 1, win_y, 1, win->element.height, win->border_color, proj); // right

    if (win->showtitlebar) {
        int titlebar_height = win->child_y_offset - 1;

        // titlebar
        ui_rect_draw(win_x, win_y, win->element.width, titlebar_height, 
                     win->highlight_title ? win->border_hl_color : win->border_color, proj);

        // caption
        if (dx_push_scissor(win_x + 1, win_y + 1, win_x + 1 + win->element.width - 1, win_y + 1 + win->child_y_offset - 2)) {
            ui_font_render_text(win_font, proj, win_x + 3, win_y + 3, win->caption, strlen(win->caption), win->titlebar_text_color);
            dx_pop_scissor();
        }
    } else {
        // top border
        ui_rect_draw(win_x, win_y, win->element.width, 1, win->border_color, proj);
    }

    // resize box
    if (win->resizable && win->draw_resizer) {
        ui_rect_draw(win_x + win->element.width - 11, win_y + win->element.height - 11,
                     10, 10, win->border_hl_color, proj);
    }
}

ui_window_t *ui_window_new(const char *caption, int x, int y) {
    ui_window_t *win = egoverlay_calloc(1, sizeof(ui_window_t));

    win->caption = egoverlay_calloc(strlen(caption)+1, sizeof(char));
    memcpy(win->caption, caption, strlen(caption));

    win->element.draw = &ui_window_draw;
    win->element.process_mouse_event = &ui_window_process_mouse_event;
    win->element.free = &ui_window_free;

    win->element.x = x;
    win->element.y = y;
    win->element.width = 100;
    win->element.height = 100;
    win->child_x_offset = 2;
    
    win->child_width = 100 - 4;
    win->child_height = 100 - win->child_y_offset - 2;

    win->vanchor = -1;
    win->hanchor = -1;

    GET_APP_SETTING_INT("overlay.ui.colors.windowBG",              (int*)&win->bg_color);
    GET_APP_SETTING_INT("overlay.ui.colors.windowBorder",          (int*)&win->border_color);
    GET_APP_SETTING_INT("overlay.ui.colors.windowBorderHighlight", (int*)&win->border_hl_color);
    GET_APP_SETTING_INT("overlay.ui.colors.text",                  (int*)&win->titlebar_text_color);

    if (!win_font) {
        char *font_path;
        int font_size = 0;
        int font_weight = INT_MIN;

        GET_APP_SETTING_STR("overlay.ui.font.path", &font_path);
        GET_APP_SETTING_INT("overlay.ui.font.size", &font_size);
        GET_APP_SETTING_INT("overlay.ui.font.weight", &font_weight);
        win_font = ui_font_get(font_path, font_size, font_weight, INT_MIN, INT_MIN);
    }

    ui_window_set_showtitlebar(win, 1);
    
    ui_element_ref(win);

    return win;
}

void ui_window_free(ui_window_t *window) {
    if (window->child) ui_element_unref(window->child);

    if (window->settings) settings_unref(window->settings);
    if (window->settings_path) egoverlay_free(window->settings_path);

    egoverlay_free(window->caption);
    egoverlay_free(window);
}

void ui_window_draw(ui_window_t *win, int offset_x, int offset_y, mat4f_t *proj) {
    int cwidth = 0;
    int cheight = 0;

    if (
        !win->resizable &&
        win->child &&
        win->child->get_preferred_size &&
        win->child->get_preferred_size(win->child, &cwidth, &cheight)
    ) {
        win->element.width  = cwidth + 4;
        win->element.height = cheight + win->child_y_offset + 2;
    }

    if (win->element.min_width > win->element.width) win->element.width = win->element.min_width;
    if (win->element.min_height > win->element.height) win->element.height = win->element.min_height;

    win->child_width = win->element.width - 4;
    win->child_height = win->element.height - win->child_y_offset - 2;

    int actual_offset_x = offset_x;
    int actual_offset_y = offset_y;

    if (win->hanchor>0) {
        // right anchor
        actual_offset_x -= win->element.width;
    } else if (win->hanchor==0) {
        // center anchor
        actual_offset_x -= (win->element.width / 2);
    } // else keep left anchor

    if (win->vanchor>0) {
        // bottom anchor
        actual_offset_y -= win->element.height;
    } else if (win->vanchor==0) {
        // center anchor
        actual_offset_y -= (win->element.height / 2);
    } // else keep top anchor

    ui_element_set_size(win->child, win->child_width,  win->child_height);

    // draw window decoration
    ui_window_draw_decorations(win, actual_offset_x, actual_offset_y, proj);

    ui_add_input_element(actual_offset_x, actual_offset_y, win->element.x, win->element.y,
                         win->element.width, win->element.height, (ui_element_t*)win);

    //if (win->resizable) ui_element_draw(win->resize_patch, dec_x, dec_y, proj);

    int coffx = win->element.x + actual_offset_x + win->child_x_offset;
    int coffy = win->element.y + actual_offset_y + win->child_y_offset;

    if (dx_push_scissor(coffx, coffy, coffx + win->child_width, coffy + win->child_height)) {
        if (win->child) ui_element_draw(win->child, coffx, coffy, proj);
        dx_pop_scissor();
    }
}

static int ui_window_process_mouse_event(ui_window_t *win, ui_mouse_event_t *event, int offset_x, int offset_y) {
    if (event->event==UI_MOUSE_EVENT_TYPE_ENTER) {
        return 0;
    }
    if (event->event==UI_MOUSE_EVENT_TYPE_LEAVE) {
        if (!win->moving) win->highlight_title = 0;
        //ui_rect_set_color(win->resize_patch, ui_window_border_color());
        return 0;
    }

    if (event->event==UI_MOUSE_EVENT_TYPE_BTN_DOWN) {
        ui_move_element_to_top(win);
        ui_grab_focus(win);
    }

    if (win->moving && event->event==UI_MOUSE_EVENT_TYPE_MOVE) {
        int delta_x = event->x - win->move_last_x;
        int delta_y = event->y - win->move_last_y;
        win->move_last_x = event->x;
        win->move_last_y = event->y;

        win->element.x += delta_x;
        win->element.y += delta_y;
        ui_window_save_to_settings(win);

        return 1;
    } else if (win->resizing && event->event==UI_MOUSE_EVENT_TYPE_MOVE) {
        int delta_x = event->x - win->move_last_x;
        int delta_y = event->y - win->move_last_y;
        win->move_last_x = event->x;
        win->move_last_y = event->y;

        win->element.width += delta_x;
        win->element.height += delta_y;
        win->child_width = win->element.width - 4;
        win->child_height = win->element.height - 2 - win->child_y_offset;
        ui_window_save_to_settings(win);
        //ui_window_update_decorations(win);
        return 1;
    }

    if (win->moving && event->event==UI_MOUSE_EVENT_TYPE_BTN_UP && event->button==UI_MOUSE_EVENT_BUTTON_LEFT) {
        win->moving = 0;
        ui_release_mouse_events((ui_element_t*)win);
        return 1;
    } else if (win->resizing && event->event==UI_MOUSE_EVENT_TYPE_BTN_UP && event->button==UI_MOUSE_EVENT_BUTTON_LEFT) {
        win->resizing = 0;
        ui_release_mouse_events((ui_element_t*)win);
        return 1;
    }

    if (
        event->x >= win->element.x &&
        event->x <= win->element.x + win->element.width &&
        event->y >= win->element.y &&
        event->y <= win->element.y + win->child_y_offset - 1
    ) {
        if (event->event==UI_MOUSE_EVENT_TYPE_MOVE) {
            //ui_rect_set_color(win->title_bar, 0x3d5a78ff);
            win->highlight_title = 1;
        }
        if (event->event==UI_MOUSE_EVENT_TYPE_BTN_DOWN && event->button==UI_MOUSE_EVENT_BUTTON_LEFT) {
            win->moving = 1;
            ui_capture_mouse_events((ui_element_t*)win, offset_x, offset_y);
            win->move_last_x = event->x;
            win->move_last_y = event->y;
        }
        
        return 1;
    } else {
        if (event->event==UI_MOUSE_EVENT_TYPE_MOVE) win->highlight_title = 0;
    }

    int resizer_x = offset_x + win->element.x + win->element.width - 11;
    int resizer_y = offset_y + win->element.y + win->element.height - 11;

    if (win->resizable && MOUSE_POINT_IN_RECT(event->x, event->y, resizer_x, resizer_y, 10, 10)) {
        if (event->event==UI_MOUSE_EVENT_TYPE_MOVE) {
            win->draw_resizer = 1;
        } else if (event->event==UI_MOUSE_EVENT_TYPE_BTN_DOWN && event->button==UI_MOUSE_EVENT_BUTTON_LEFT) {
            win->resizing = 1;
            ui_capture_mouse_events((ui_element_t*)win, offset_x, offset_y);
            win->move_last_x = event->x;
            win->move_last_y = event->y;
        }
    } else {
        win->draw_resizer = 0;
    }
    

    return 1;
}

void ui_window_set_showtitlebar(ui_window_t *win, int showtitlebar) {
    win->showtitlebar = showtitlebar ? 1 : 0;

    if (win->showtitlebar) {
        char *font_path;
        int font_size = 0;
        int font_weight = INT_MIN;
        GET_APP_SETTING_STR("overlay.ui.font.path", &font_path);
        GET_APP_SETTING_INT("overlay.ui.font.size", &font_size);
        GET_APP_SETTING_INT("overlay.ui.font.weight", &font_weight);

        ui_font_t *font = ui_font_get(font_path, font_size, font_weight, INT_MIN, INT_MIN);

        win->child_y_offset = ui_font_get_line_spacing(font) + 6;
    } else {
        win->child_y_offset = 2;
    }
}

void ui_window_set_child(ui_window_t *window, ui_element_t *child) {
    if (window->child) ui_element_unref(window->child);
    window->child = child;
    ui_element_ref(child);
}


void ui_window_show(ui_window_t *window) {
    if (window->shown) return;
    ui_add_top_level_element(window);
    window->shown = 1;
}

void ui_window_hide(ui_window_t *window) {
    if (!window->shown) return;
    ui_remove_top_level_element(window);
    window->shown = 0;
}

void ui_window_set_resizable(ui_window_t *window, int resizable) {
    window->resizable = resizable;
}

void ui_window_set_autosize(ui_window_t *window, int autosize) {
    window->autosize = autosize;
    if (autosize) window->resizable = 0;
}

void ui_window_update_from_settings(ui_window_t *win) {
    if (!win->settings) return;

    char *x_path = egoverlay_calloc(strlen(win->settings_path) + strlen(".x") + 1, sizeof(char));
    char *y_path = egoverlay_calloc(strlen(win->settings_path) + strlen(".y") + 1, sizeof(char));
    char *w_path = egoverlay_calloc(strlen(win->settings_path) + strlen(".width") + 1, sizeof(char));
    char *h_path = egoverlay_calloc(strlen(win->settings_path) + strlen(".height") + 1, sizeof(char));

    memcpy(x_path, win->settings_path, strlen(win->settings_path));
    memcpy(x_path + strlen(win->settings_path), ".x", 2);

    memcpy(y_path, win->settings_path, strlen(win->settings_path));
    memcpy(y_path + strlen(win->settings_path), ".y", 2);

    memcpy(w_path, win->settings_path, strlen(win->settings_path));
    memcpy(w_path + strlen(win->settings_path), ".width", strlen(".width"));

    memcpy(h_path, win->settings_path, strlen(win->settings_path));
    memcpy(h_path + strlen(win->settings_path), ".height", strlen(".height"));

    settings_get_int(win->settings, x_path, &win->element.x);
    settings_get_int(win->settings, y_path, &win->element.y);
    settings_get_int(win->settings, w_path, &win->element.width);
    settings_get_int(win->settings, h_path, &win->element.height);

    egoverlay_free(x_path);
    egoverlay_free(y_path);
    egoverlay_free(w_path);
    egoverlay_free(h_path);
}

void ui_window_save_to_settings(ui_window_t *win) {
    if (!win->settings) return;

    char *x_path = egoverlay_calloc(strlen(win->settings_path) + strlen(".x") + 1, sizeof(char));
    char *y_path = egoverlay_calloc(strlen(win->settings_path) + strlen(".y") + 1, sizeof(char));
    char *w_path = egoverlay_calloc(strlen(win->settings_path) + strlen(".width") + 1, sizeof(char));
    char *h_path = egoverlay_calloc(strlen(win->settings_path) + strlen(".height") + 1, sizeof(char));

    memcpy(x_path, win->settings_path, strlen(win->settings_path));
    memcpy(x_path + strlen(win->settings_path), ".x", 2);

    memcpy(y_path, win->settings_path, strlen(win->settings_path));
    memcpy(y_path + strlen(win->settings_path), ".y", 2);

    memcpy(w_path, win->settings_path, strlen(win->settings_path));
    memcpy(w_path + strlen(win->settings_path), ".width", strlen(".width"));

    memcpy(h_path, win->settings_path, strlen(win->settings_path));
    memcpy(h_path + strlen(win->settings_path), ".height", strlen(".height"));

    settings_set_int(win->settings, x_path, win->element.x);
    settings_set_int(win->settings, y_path, win->element.y);
    settings_set_int(win->settings, w_path, win->element.width);
    settings_set_int(win->settings, h_path, win->element.height);

    egoverlay_free(x_path);
    egoverlay_free(y_path);
    egoverlay_free(w_path);
    egoverlay_free(h_path);
}

void lua_pushuiwindow(lua_State *L, ui_window_t *window);

int ui_window_lua_new(lua_State *L);
int ui_window_lua_del(lua_State *L);
int ui_window_lua_show(lua_State *L);
int ui_window_lua_hide(lua_State *L);
int ui_window_lua_min_size(lua_State *L);
int ui_window_lua_resizable(lua_State *L);
int ui_window_lua_settings(lua_State *L);
int ui_window_lua_position(lua_State *L);
int ui_window_lua_caption(lua_State *L);
int ui_window_lua_hanchor(lua_State *L);
int ui_window_lua_vanchor(lua_State *L);
int ui_window_lua_titlebar(lua_State *L);

int ui_window_lua_set_child(lua_State *L);

luaL_Reg window_funcs[] = {
    "__gc"     , &ui_window_lua_del,
    "set_child", &ui_window_lua_set_child,
    "show"     , &ui_window_lua_show,
    "hide"     , &ui_window_lua_hide,
    "min_size" , &ui_window_lua_min_size,
    "resizable", &ui_window_lua_resizable,
    "settings" , &ui_window_lua_settings,
    "position" , &ui_window_lua_position,
    "caption"  , &ui_window_lua_caption,
    "hanchor"  , &ui_window_lua_hanchor,
    "vanchor"  , &ui_window_lua_vanchor,
    "titlebar" , &ui_window_lua_titlebar,
    NULL       ,  NULL
};

void ui_window_lua_register_ui_funcs(lua_State *L) {
    lua_pushcfunction(L, &ui_window_lua_new);
    lua_setfield(L, -2, "window");
}

void lua_ui_window_register_metatable(lua_State *L) {
    if (luaL_newmetatable(L, "UIWindowMetaTable")) {
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
        luaL_setfuncs(L, window_funcs, 0);
    }
}

void lua_pushuiwindow(lua_State *L, ui_window_t *window) {
    ui_element_ref(window);

    ui_window_t **win = lua_newuserdata(L, sizeof(ui_window_t*));
    *win = window;

    lua_ui_window_register_metatable(L);
    lua_setmetatable(L, -2);
}

/*** RST
Windows
=======

.. lua:currentmodule:: eg-overlay-ui

Functions
---------

.. lua:function:: window(caption, x, y)

    Create a new :lua:class:`uiwindow`.

    :param caption: The window title.
    :type caption: string
    :param x: Initial x position.
    :type x: integer
    :param y: Initial y position.
    :type y: integer
    :return: A new window element.

    .. versionhistory::
        :0.0.1: Added
*/

int ui_window_lua_new(lua_State *L) {
    const char *caption = luaL_checkstring(L, 1);
    int x = (int)luaL_checkinteger(L, 2);
    int y = (int)luaL_checkinteger(L, 3);

    ui_window_t *win = ui_window_new(caption, x, y);

    lua_pushuiwindow(L, win);
    ui_element_unref(win);

    return 1;
}

ui_window_t *lua_checkuiwindow(lua_State *L, int ind) {
    return *(ui_window_t**)luaL_checkudata(L, ind, "UIWindowMetaTable");
}

int ui_window_lua_del(lua_State *L) {
    ui_window_t *win = lua_checkuiwindow(L, 1);

    ui_element_unref(win);

    return 0;
}

/*** RST
Classes
-------

.. lua:class:: uiwindow

    A top-level window element. A window can only have a single child, which
    should be a layout container such as a :lua:class:`uibox`.

    .. lua:method:: set_child(uielement)

        Set the child element of this window.

        :param uielement: A ui element.
        
        .. versionhistory::
            :0.0.1: Added
*/
int ui_window_lua_set_child(lua_State *L) {
    ui_window_t *win = lua_checkuiwindow(L, 1);

    ui_element_t *element = lua_checkuielement(L, 2);

    ui_window_set_child(win, (ui_element_t*)element);

    return 0;
}

/*** RST
    .. lua:method:: show()

        Show the window. If the window is already visible, this function has no
        effect.

        .. versionhistory::
            :0.0.1: Added
*/
int ui_window_lua_show(lua_State *L) {
    ui_window_t *win = lua_checkuiwindow(L, 1);

    ui_window_show(win);

    return 0;
}

/*** RST
    .. lua:method:: hide()

        Hide the window.

        .. versionhistory::
            :0.0.1: Added
*/
int ui_window_lua_hide(lua_State *L) {
    ui_window_t *win = lua_checkuiwindow(L, 1);

    ui_window_hide(win);
    
    return 0;
}

/*** RST
    .. lua:method:: min_size(width, height)

        Set the minimum size for the window.

        :param width:
        :type width: integer
        :param height:
        :type height: integer

        .. versionhistory::
            :0.0.1: Added
*/
int ui_window_lua_min_size(lua_State *L) {
    ui_window_t *win = lua_checkuiwindow(L, 1);
    int width = (int)luaL_checkinteger(L, 2);
    int height = (int)luaL_checkinteger(L, 3);

    win->element.min_width = width;
    win->element.min_height = height;

    return 0;
}

/*** RST
    .. lua:method:: resizable(value)

        Set wether or not the window should be resizable by the user.

        :param value: True if the user should be able to resize the window.
        :type value: boolean

        .. versionhistory::
            :0.0.1: Added
*/
int ui_window_lua_resizable(lua_State *L) {
    ui_window_t *win = lua_checkuiwindow(L, 1);
    int r = lua_toboolean(L, 2);

    ui_window_set_resizable(win, r);

    return 0;
}

/*** RST
    .. lua:method:: settings(settings, path)

        Bind a window to a settings object. This causes the window update its
        size and position to the values in the settings object and then save
        any changes made after.

        This allows a window position and size to be persisted between overlay
        sessions.

        ``path`` is a base key below which the window will save position and size
        to individual keys:

        ====== ===================
        Key    Value
        ====== ===================
        x      Horizontal position
        y      Vertical position
        width  Width
        height Height
        ====== ===================

        :param settings: A :lua:class:`settings` object
        :type settings: settings
        :param path: The base settings path

        .. code-block:: lua
            :caption: Example

            local console_settings = settings.new("console.lua")

            console_settings:set_default("window.x", 200)
            console_settings:set_default("window.y", 30)
            console_settings:set_default("window.width", 600)
            console_settings:set_default("window.height", 300)

            console.win = ui.window("Lua Console/Log", 0, 0)
            console.win:min_size(600, 300)
            console.win:resizable(true)
            console.win:settings(console_settings, "window")
        
        .. versionhistory::
            :0.0.1: Added
*/
int ui_window_lua_settings(lua_State *L) {
    ui_window_t *win = lua_checkuiwindow(L, 1);
    settings_t *settings = lua_checksettings(L, 2);
    const char *path = luaL_checkstring(L, 3);

    win->settings = settings;

    if (win->settings_path) egoverlay_free(win->settings_path);
    win->settings_path = egoverlay_calloc(strlen(path)+1, sizeof(char));
    memcpy(win->settings_path, path, strlen(path));

    settings_ref(settings);

    ui_window_update_from_settings(win);

    return 0;
}

/*** RST
    .. lua:method:: position(x, y)

        Set the window position.

        :param integer x:
        :param integer y:

        .. versionhistory::
            :0.1.0: Added
*/
int ui_window_lua_position(lua_State *L) {
    ui_window_t *win = lua_checkuiwindow(L, 1);
    int x = (int)luaL_checkinteger(L, 2);
    int y = (int)luaL_checkinteger(L, 3);

    win->element.x = x;
    win->element.y = y;

    return 0;
}

/*** RST
    .. lua:method:: caption(text)

        Set the window caption

        :param string text:

        .. versionhistory::
            :0.1.0: Added
*/
int ui_window_lua_caption(lua_State *L) {
    ui_window_t *win = lua_checkuiwindow(L, 1);
    const char *newcaption = luaL_checkstring(L, 2);

    egoverlay_free(win->caption);
    win->caption = egoverlay_calloc(strlen(newcaption)+1, sizeof(char));
    memcpy(win->caption, newcaption, strlen(newcaption));

    return 0;
}

/*** RST
    .. lua:method:: hanchor(anchor)
    
        Set the horizontal anchor position for the window. The horizontal anchor
        controls how the window position is used to position the window.
        By default, the window is anchored to the top-left.

        :param integer anchor: The new anchor position: ``-1`` for left, ``0``
            for middle, ``1`` for right.
        
        .. versionhistory::
            :0.1.0: Added
*/
int ui_window_lua_hanchor(lua_State *L) {
    ui_window_t *win = lua_checkuiwindow(L, 1);

    win->hanchor = (int)luaL_checkinteger(L, 2);

    return 0;
}

/*** RST
    .. lua:method:: vanchor(anchor)
    
        Set the vertical anchor position for the window. The vertical anchor
        controls how the window position is used to position the window.
        By default, the window is anchored to the top-left.

        :param integer anchor: The new anchor position: ``-1`` for top, ``0``
            for middle, ``1`` for bottom.
        
        .. versionhistory::
            :0.1.0: Added
*/
int ui_window_lua_vanchor(lua_State *L) {
    ui_window_t *win = lua_checkuiwindow(L, 1);

    win->vanchor = (int)luaL_checkinteger(L, 2);

    return 0;
}

/*** RST
    .. lua:method:: titlebar(show)

        Set whether or not the titlebar is shown.

        :param boolean show:

        .. versionhistory::
            :0.1.0: Added
*/
int ui_window_lua_titlebar(lua_State *L) {
    ui_window_t *win = lua_checkuiwindow(L, 1);

    ui_window_set_showtitlebar(win, lua_toboolean(L, 2));

    return 0;
}

/*

static int ui_window_lua_set_autosize(lua_State *L) {
    ui_window_t *win = CHECK_UI_WIN(L, 1);
    int a = lua_toboolean(L, 2);

    ui_window_set_autosize(win, a);

    return 0;
}
*/
