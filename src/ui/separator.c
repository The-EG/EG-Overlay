#include <stdlib.h>
#include "ui.h"
#include "separator.h"
#include "rect.h"
#include "../app.h"
#include <lauxlib.h>

struct ui_separator_t {
    ui_element_t element;

    // 0 vertical, 1 horizontal
    int orientation;

    int thickness;
    ui_color_t color;
};

ui_separator_t *ui_separator_new(int orientation);
void ui_separator_free(ui_separator_t *sep);

void ui_separator_draw(ui_separator_t *sep, int offset_x, int offset_y, mat4f_t *proj);
int ui_separator_get_preferred_size(ui_separator_t *sep, int *width, int *height);

ui_separator_t *ui_separator_new(int orientation) {
    ui_separator_t *sep = calloc(1, sizeof(ui_separator_t));

    sep->orientation = orientation;
    sep->thickness = 1;
    GET_APP_SETTING_INT("overlay.ui.colors.windowBorder", (int*)&sep->color);

    sep->element.draw               = &ui_separator_draw;
    sep->element.get_preferred_size = &ui_separator_get_preferred_size;
    sep->element.free               = &ui_separator_free;

    ui_element_ref(sep);
    
    return sep;
}

void ui_separator_free(ui_separator_t *sep) {
    free(sep);
}

void ui_separator_draw(ui_separator_t *sep, int offset_x, int offset_y, mat4f_t *proj) {
    if (sep->element.width <= 0 || sep->element.height <= 0)  return;

    int width = sep->element.width - (sep->orientation ? 0 : 2);
    int height = sep->element.height - (sep->orientation ? 2 : 0);

    ui_rect_draw(offset_x + sep->element.x, offset_y + sep->element.y, width, height, sep->color, proj);
}

int ui_separator_get_preferred_size(ui_separator_t *sep, int *width, int *height) {
    if (sep->orientation == 0) {
        *width = sep->thickness + 2;
        *height = 20;
    } else {
        *height = sep->thickness + 2;
        *width = 20;
    }
    return 1;
}

void lua_push_ui_separator(lua_State *L, ui_separator_t *sep);

int ui_separator_lua_new(lua_State *L);
int ui_separator_lua_del(lua_State *L);
int ui_separator_lua_thickness(lua_State *L);
int ui_separator_lua_color(lua_State *L);

luaL_Reg ui_separator_funcs[] = {
    "__gc"     , &ui_separator_lua_del,
    "thickness", &ui_separator_lua_thickness,
    "color"    , &ui_separator_lua_color,
    NULL       ,  NULL
};

void ui_separator_lua_register_ui_funcs(lua_State *L) {
    lua_pushcfunction(L, &ui_separator_lua_new);
    lua_setfield(L, -2, "separator");
}

void lua_push_ui_separator(lua_State *L, ui_separator_t *sep) {
    ui_separator_t **psep = (ui_separator_t**)lua_newuserdata(L, sizeof(ui_separator_t*));
    *psep = sep;

    if (luaL_newmetatable(L, "UISeparatorMetaTable")) {
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
        luaL_setfuncs(L, ui_separator_funcs, 0);
    }
    lua_setmetatable(L, -2);

    ui_element_ref(sep);
}

/*** RST
Separator Element
=================

.. lua:currentmodule:: overlay-ui

Functions
---------

.. lua:function:: separator(orientation)

    Create a new :lua:class:`uiseparator`.

    :param orientation: Either ``'horizontal'`` or ``'vertical'``.
    :type orientation: string
    :rtype: uiseparator

    .. versionhistory::
        :0.0.1: Added
*/

int ui_separator_lua_new(lua_State *L) {
    const char *orientation_str = luaL_checkstring(L, 1);

    int orientation = 0;
    if      (strcmp(orientation_str, "horizontal")==0) orientation = 1;
    else if (strcmp(orientation_str, "vertical"  )==0) orientation = 0;
    else return luaL_error(L, "ui.separator(orientation) - orientation must be 'horizontal' or 'vertical'.");

    ui_separator_t *sep = ui_separator_new(orientation);
    lua_push_ui_separator(L, sep);
    ui_element_unref(sep);

    return 1;
}

/*** RST
Classes 
-------

.. lua:class:: uiseparator

    A separator element. This element draws either a horizontal or vertical line,
    that serves as a separator between other elements. It is typically used in a
    box layout element to separate items.

*/

#define LUA_CHECK_SEP(L, ind) *(ui_separator_t**)luaL_checkudata(L, ind, "UISeparatorMetaTable")

int ui_separator_lua_del(lua_State *L) {
    ui_separator_t *sep = LUA_CHECK_SEP(L, 1);
    ui_element_unref(sep);
    return 0;
}

/*** RST
    .. lua:method:: thickness(value)

        Set how thick the separator line is drawn, in pixels.

        :param value:
        :type value: integer

        .. versionhistory::
            :0.0.1: Added
*/
int ui_separator_lua_thickness(lua_State *L) {
    ui_separator_t *sep = LUA_CHECK_SEP(L, 1);
    int thickness = (int)luaL_checkinteger(L, 2);

    sep->thickness = thickness;

    return 0;
}

/*** RST
    .. lua:method:: color(value)

        Set the color of the separator.

        :param value: The new color. See :ref:`colors`.
        :type value: integer

        .. versionhistory::
            :0.0.1: Added
*/
int ui_separator_lua_color(lua_State *L) {
    ui_separator_t *sep = LUA_CHECK_SEP(L, 1);
    ui_color_t color = (ui_color_t)luaL_checkinteger(L, 2);

    sep->color = color;

    return 0;
}