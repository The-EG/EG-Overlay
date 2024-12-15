#include <stdlib.h>
#include <lauxlib.h>
#include "scrollview.h"
#include "../utils.h"
#include "../logging/logger.h"
#include "rect.h"
#include "../app.h"
#include "../dx.h"

struct ui_scroll_view_t {
    ui_element_t element;

    ui_element_t *child;

    int disp_x;
    int disp_y;

    int show_vertical_bar;
    int highlight_vertical_bar;
    int vert_bar_size;
    int vert_bar_y;
    int vert_bar_range;
    float vert_bar_scale;

    int show_horiz_bar;
    int highlight_horiz_bar;
    int horiz_bar_size;
    int horiz_bar_x;
    int horiz_bar_range;
    float horiz_bar_scale;

    int last_mouse_x;
    int last_mouse_y;

    int mouse_scroll_y;
    int mouse_scroll_x;
    int scroll_amount;
};

void ui_scroll_view_free(ui_scroll_view_t *scroll);

void ui_scroll_view_draw(ui_scroll_view_t *scroll, int offset_x, int offset_y, mat4f_t *proj);
int ui_scroll_view_get_preferred_size(ui_scroll_view_t *scroll, int *width, int *height);
int ui_scroll_view_process_mouse_event(ui_scroll_view_t *scroll, ui_mouse_event_t *event, int offset_x, int offset_y);
void ui_scroll_view_size_updated(ui_scroll_view_t *scroll);

ui_scroll_view_t *ui_scroll_view_new() {
    ui_scroll_view_t *scroll = egoverlay_calloc(1, sizeof(ui_scroll_view_t));
    scroll->element.width = 100;
    scroll->element.height = 100;
    scroll->element.draw = &ui_scroll_view_draw;
    scroll->element.get_preferred_size = &ui_scroll_view_get_preferred_size;
    scroll->element.process_mouse_event = &ui_scroll_view_process_mouse_event;
    scroll->element.free = &ui_scroll_view_free;
    scroll->scroll_amount = 20;

    ui_element_ref(scroll);

    return scroll;
}

void ui_scroll_view_free(ui_scroll_view_t *scroll) {
    if (scroll->child) ui_element_unref(scroll->child);

    egoverlay_free(scroll);
}

void ui_scroll_view_set_child(ui_scroll_view_t *scroll, ui_element_t *child) {
    if (scroll->child) ui_element_unref(scroll->child);
    scroll->child = child;
    if (child) ui_element_ref(child);
}

void ui_scroll_view_draw(ui_scroll_view_t *scroll, int offset_x, int offset_y, mat4f_t *proj) {
    int sx = scroll->element.x + offset_x;
    int sy = scroll->element.y + offset_y;

    if (dx_push_scissor(sx, sy, sx + scroll->element.width, sy + scroll->element.height)) {
        ui_add_input_element(offset_x, offset_y, scroll->element.x, scroll->element.y,
                             scroll->element.width, scroll->element.height, (ui_element_t*)scroll);
        if (scroll->child) {
            int cwidth = 0;
            int cheight = 0;

            if (
                scroll->child->get_preferred_size &&
                scroll->child->get_preferred_size(scroll->child, &cwidth, &cheight)
            ) {
                if (scroll->disp_x < 0) scroll->disp_x = 0;
                
                if (cwidth > scroll->element.width) {
                    if (scroll->disp_x > cwidth - scroll->element.width) {
                        scroll->disp_x = cwidth - scroll->element.width;
                    }
                } else scroll->disp_x = 0;

                if (scroll->disp_y < 0) scroll->disp_y = 0;

                if (cheight > scroll->element.height) {
                    if (scroll->disp_y > cheight - scroll->element.height) {
                        scroll->disp_y = cheight - scroll->element.height;
                    }
                } else scroll->disp_y = 0;

                ui_element_set_size(scroll->child, cwidth, cheight);
            }

            int cx = sx - scroll->disp_x;
            int cy = sy - scroll->disp_y;

            ui_element_draw(scroll->child, cx, cy, proj);

            if (scroll->show_vertical_bar) {
                ui_color_t border_color = 0;
                ui_color_t border_highlight_color = 0;

                GET_APP_SETTING_INT("overlay.ui.colors.windowBorder",          (int*)&border_color);
                GET_APP_SETTING_INT("overlay.ui.colors.windowBorderHighlight", (int*)&border_highlight_color);

                scroll->vert_bar_size = (int)(((float)scroll->element.height / (float)cheight) *
                                              scroll->element.height);
                scroll->vert_bar_range = scroll->element.height - scroll->vert_bar_size;
                scroll->vert_bar_scale = (float)scroll->disp_y / (float)(cheight - scroll->element.height);
                scroll->vert_bar_y = (int)(scroll->vert_bar_range * scroll->vert_bar_scale);
                
                ui_rect_draw(sx + scroll->element.width - 11, sy + scroll->vert_bar_y, 10, scroll->vert_bar_size,
                             scroll->highlight_vertical_bar ? border_highlight_color : border_color, proj);
            }

            if (scroll->show_horiz_bar) {
                ui_color_t border_color = 0;
                ui_color_t border_highlight_color = 0;

                GET_APP_SETTING_INT("overlay.ui.colors.windowBorder",          (int*)&border_color);
                GET_APP_SETTING_INT("overlay.ui.colors.windowBorderHighlight", (int*)&border_highlight_color);

                scroll->horiz_bar_size = (int)(((float)scroll->element.width / (float)cwidth) * scroll->element.width);
                scroll->horiz_bar_range = scroll->element.width - scroll->horiz_bar_size;
                scroll->horiz_bar_scale = (float)scroll->disp_x / (float)(cwidth - scroll->element.width);
                scroll->horiz_bar_x = (int)(scroll->horiz_bar_range * scroll->horiz_bar_scale);
                
                ui_rect_draw(sx + scroll->horiz_bar_x, sy + scroll->element.height - 11,
                             scroll->horiz_bar_size, 10,
                             scroll->highlight_horiz_bar ? border_highlight_color : border_color, proj);
            }
        }

        dx_pop_scissor();
    }
}

int ui_scroll_view_get_preferred_size(ui_scroll_view_t *scroll, int *width, int *height) {
    UNUSED_PARAM(scroll);

    *width = 0; //scroll->element.width;
    *height = 0; //scroll->element.height;

    return 1;
}

int ui_scroll_view_process_mouse_event(ui_scroll_view_t *scroll, ui_mouse_event_t *event, int offset_x, int offset_y) {
    if (event->event==UI_MOUSE_EVENT_TYPE_WHEEL) {
        scroll->disp_y -= event->value * scroll->scroll_amount;
        return 1;
    } else if (event->event==UI_MOUSE_EVENT_TYPE_HWHEEL) {
        scroll->disp_x += event->value * scroll->scroll_amount;
        return 1;
    }

    int vert_scroll_region_x = offset_x + scroll->element.x + scroll->element.width - 11;
    int vert_scroll_region_y = offset_y + scroll->element.y;
    int horiz_scroll_region_x = offset_x + scroll->element.x;
    int horiz_scroll_region_y = offset_y + scroll->element.y + scroll->element.height - 11;

    if (event->event==UI_MOUSE_EVENT_TYPE_LEAVE) {
        if (!scroll->mouse_scroll_y) scroll->show_vertical_bar = 0;
        if (!scroll->mouse_scroll_x) scroll->show_horiz_bar = 0;
        return 0;
    }

    if (event->event==UI_MOUSE_EVENT_TYPE_MOVE && !scroll->mouse_scroll_y) {
        if (MOUSE_POINT_IN_RECT(
                event->x,
                event->y,
                vert_scroll_region_x,
                vert_scroll_region_y,
                11,
                scroll->element.height
        )) {
            scroll->show_vertical_bar = 1;

            if (MOUSE_POINT_IN_RECT(
                    event->x,
                    event->y,
                    vert_scroll_region_x,
                    vert_scroll_region_y + scroll->vert_bar_y,
                    10,
                    scroll->vert_bar_size
            )) {
                scroll->highlight_vertical_bar = 1;
            } else {
                scroll->highlight_vertical_bar = 0;
            }

            return 1;
        } else {
            scroll->show_vertical_bar = 0;
            scroll->highlight_vertical_bar = 0;
        }
    }

    if (event->event==UI_MOUSE_EVENT_TYPE_MOVE && !scroll->mouse_scroll_x) {
        if (MOUSE_POINT_IN_RECT(event->x, event->y, horiz_scroll_region_x, horiz_scroll_region_y, scroll->element.width, 11)) {
            scroll->show_horiz_bar = 1;

            if (MOUSE_POINT_IN_RECT(
                    event->x,
                    event->y,
                    horiz_scroll_region_x + scroll->horiz_bar_x,
                    horiz_scroll_region_y,
                    scroll->horiz_bar_size,
                    10
            )) {
                scroll->highlight_horiz_bar = 1;
            } else {
                scroll->highlight_horiz_bar = 0;
            }
            
            return 1;
        } else {
            scroll->show_horiz_bar = 0;
            scroll->highlight_horiz_bar = 0;
        }
    }

    if (event->event==UI_MOUSE_EVENT_TYPE_MOVE && scroll->mouse_scroll_y) {
        int deltay = event->y - scroll->last_mouse_y;

        int sb_range = scroll->element.height - scroll->vert_bar_size;
        int child_range = scroll->child->height - scroll->element.height;

        float sb_move_scale = (float)child_range / (float)sb_range;

        scroll->disp_y += (int)(deltay * sb_move_scale);

        scroll->last_mouse_y = event->y;
        scroll->last_mouse_x = event->x;
        return 1;
    }

    if (event->event==UI_MOUSE_EVENT_TYPE_MOVE && scroll->mouse_scroll_x) {
        int deltax = event->x - scroll->last_mouse_x;

        int sb_range = scroll->element.width - scroll->horiz_bar_size;
        int child_range = scroll->child->width - scroll->element.width;

        float sb_move_scale = (float)child_range / (float)sb_range;

        scroll->disp_x += (int)(deltax * sb_move_scale);

        scroll->last_mouse_y = event->y;
        scroll->last_mouse_x = event->x;
        return 1;
    }

    if (scroll->show_vertical_bar && !scroll->mouse_scroll_y && !scroll->mouse_scroll_x &&
        MOUSE_POINT_IN_RECT(
            event->x,
            event->y,
            vert_scroll_region_x,
            vert_scroll_region_y + scroll->vert_bar_y,
            10, scroll->vert_bar_size
        ) &&
        event->event==UI_MOUSE_EVENT_TYPE_BTN_DOWN &&
        event->button==UI_MOUSE_EVENT_BUTTON_LEFT)
    {
        scroll->mouse_scroll_y = 1;
        scroll->last_mouse_x = event->x;
        scroll->last_mouse_y = event->y;
        ui_capture_mouse_events((ui_element_t*)scroll, offset_x, offset_y);
    }

    if (scroll->show_horiz_bar && !scroll->mouse_scroll_y && !scroll->mouse_scroll_x &&
        MOUSE_POINT_IN_RECT(
            event->x,
            event->y,
            horiz_scroll_region_x + scroll->horiz_bar_x,
            horiz_scroll_region_y,
            scroll->horiz_bar_size, 10
        ) &&
        event->event==UI_MOUSE_EVENT_TYPE_BTN_DOWN &&
        event->button==UI_MOUSE_EVENT_BUTTON_LEFT)
    {
        scroll->mouse_scroll_x = 1;
        scroll->last_mouse_x = event->x;
        scroll->last_mouse_y = event->y;
        ui_capture_mouse_events((ui_element_t*)scroll, offset_x, offset_y);
    }

    if (scroll->mouse_scroll_y && event->event==UI_MOUSE_EVENT_TYPE_BTN_UP && event->button==UI_MOUSE_EVENT_BUTTON_LEFT) {
        scroll->mouse_scroll_y = 0;
        ui_release_mouse_events((ui_element_t*)scroll);
    }

    if (scroll->mouse_scroll_x && event->event==UI_MOUSE_EVENT_TYPE_BTN_UP && event->button==UI_MOUSE_EVENT_BUTTON_LEFT) {
        scroll->mouse_scroll_x = 0;
        ui_release_mouse_events((ui_element_t*)scroll);
    }

    return 0;
}

void ui_scroll_view_scroll_y(ui_scroll_view_t *scroll, int scroll_y) {
    scroll->disp_y = scroll_y;
}

int ui_scroll_view_lua_new(lua_State *L);
int ui_scroll_view_lua_del(lua_State *L);
int ui_scroll_view_lua_size(lua_State *L);
int ui_scroll_view_lua_pos(lua_State *L);
int ui_scroll_view_lua_set_child(lua_State *L);
int ui_scroll_view_lua_scroll_max_y(lua_State *L);
int ui_scroll_view_lua_scroll_amount(lua_State *L);

void ui_scroll_view_register_lua_funcs(lua_State *L) {
    lua_pushcfunction(L, &ui_scroll_view_lua_new);
    lua_setfield(L, -2, "scrollview");
}

luaL_Reg scroll_view_funcs[] = {
    "__gc"         , &ui_scroll_view_lua_del,
    "size"         , &ui_scroll_view_lua_size,
    "pos"          , &ui_scroll_view_lua_pos,
    "set_child"    , &ui_scroll_view_lua_set_child,
    "scroll_max_y" , &ui_scroll_view_lua_scroll_max_y,
    "scroll_amount", &ui_scroll_view_lua_scroll_amount,
    "background"   , &ui_element_lua_background,
    NULL,             NULL
};

void ui_scroll_view_register_metatable(lua_State *L) {
    if (luaL_newmetatable(L, "UIScrollViewMetaTable")) {
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");

        lua_pushboolean(L, 1);
        lua_setfield(L, -2, "__is_uielement");

        luaL_setfuncs(L, scroll_view_funcs, 0);
    }
}

void lua_pushuiscrollview(lua_State *L, ui_scroll_view_t *scroll) {
    ui_element_ref(scroll);

    ui_scroll_view_t **psv = lua_newuserdata(L, sizeof(ui_scroll_view_t*));
    *psv = scroll;

    ui_scroll_view_register_metatable(L);
    lua_setmetatable(L, -2);
}

/*** RST
Scrollview Container
====================

.. lua:currentmodule:: eg-overlay-ui

Functions
---------

.. lua:function:: scrollview()

    Create a new :lua:class:`uiscrollview`.

    :rtype: uiscrollview

    .. versionhistory::
        :0.0.1: Added
*/
int ui_scroll_view_lua_new(lua_State *L) {
    ui_scroll_view_t *sv = ui_scroll_view_new();
    lua_pushuiscrollview(L, sv);
    ui_element_unref(sv);

    return 1;
}

ui_scroll_view_t *lua_checkuiscrollview(lua_State *L, int ind) {
    return *(ui_scroll_view_t**)luaL_checkudata(L, ind, "UIScrollViewMetaTable");
}

/*** RST
Classes
-------

.. lua:class:: uiscrollview

    A scrolling container.

    Unlike other containers, the scrollview does not automatically resize to fit
    its child element. Instead, it presents a scrollable view based on its size.

    Scrollviews can have a single child, which will be a layout container.

*/


int ui_scroll_view_lua_del(lua_State *L) {
    ui_scroll_view_t *sv = lua_checkuiscrollview(L, 1);

    ui_element_unref(sv);

    return 0;
}

int ui_scroll_view_lua_size(lua_State *L) {
    ui_scroll_view_t *sv = lua_checkuiscrollview(L, 1);
    int w = (int)luaL_checkinteger(L, 2);
    int h = (int)luaL_checkinteger(L, 3);

    ui_element_set_size(sv, w, h);

    return 0;
}

int ui_scroll_view_lua_pos(lua_State *L) {
    ui_scroll_view_t *sv = lua_checkuiscrollview(L, 1);
    int x = (int)luaL_checkinteger(L, 2);
    int y = (int)luaL_checkinteger(L, 3);

    ui_element_set_pos(sv, x, y);

    return 0;
}

/*** RST
    .. lua:method:: set_child(uielement)

        Set the child element.

        :param uielement: The new child element.
        
        .. versionhistory::
            :0.0.1: Added
*/
int ui_scroll_view_lua_set_child(lua_State *L) {
    ui_scroll_view_t *sv = lua_checkuiscrollview(L, 1);

    if (lua_type(L, 2)==LUA_TNIL) {
        ui_scroll_view_set_child(sv, NULL);
        return 0;
    }

    ui_element_t *child = lua_checkuielement(L, 2);

    ui_scroll_view_set_child(sv, child);

    return 0;
}

/*** RST
    .. lua:method:: scroll_max_y()

        Set the Y scroll position so that the bottom most portion of the child
        element is visible.

        .. versionhistory::
            :0.0.1: Added
*/
int ui_scroll_view_lua_scroll_max_y(lua_State *L) {
    ui_scroll_view_t *sv = lua_checkuiscrollview(L, 1);
    ui_scroll_view_scroll_y(sv, INT32_MAX);
    
    return 0;
}

/*** RST
    .. lua:method:: scroll_amount(value)

        Set the amount the view is scrolled on mouse wheel movements.

        :param value:
        :type value: integer

        .. versionhistory::
            :0.0.1: Added
*/
int ui_scroll_view_lua_scroll_amount(lua_State *L) {
    ui_scroll_view_t *sv = lua_checkuiscrollview(L, 1);
    int scroll_amount = (int)luaL_checkinteger(L, 2);
    
    sv->scroll_amount = scroll_amount;

    return 0;
}

/*** RST
    .. include:: /docs/_include/ui_element_color.rst
*/
