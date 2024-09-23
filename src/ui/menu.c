#include "../lamath.h"
#include "rect.h"
#include "text.h"
#include "ui.h"
#include "menu.h"
#include "box.h"
#include "../app.h"
#include "../utils.h"
#include "../logging/logger.h"
#include "../lua-manager.h"
#include <lauxlib.h>

typedef void menu_item_free_fn(void *menu_item);

struct ui_menu_item_t {
    ui_element_t element;

    int draw_bg;
    int enabled;

    int pre_size;
    int post_size;

    menu_item_event_callback *callback;

    ui_menu_t *sub_menu;

    ui_menu_t *parent_menu;

    ui_element_t *child;
    ui_element_t *pre;
};

struct ui_menu_t {
    ui_element_t element;

    ui_menu_t *child_menu;
    ui_menu_t *parent_menu;

    ui_box_t *box;

};

void ui_menu_free(ui_menu_t *menu);

void ui_menu_item_draw(ui_menu_item_t *item, int offset_x, int offset_y, mat4f_t *proj);
void ui_menu_item_free(ui_menu_item_t *item);
int ui_menu_item_get_preferred_size(ui_menu_item_t *item, int *width, int *height);
int ui_menu_item_process_mouse_event(ui_menu_item_t *item, ui_mouse_event_t *event, int offset_x, int offset_y);

int ui_menu_process_mouse_event(ui_menu_t *menu, ui_mouse_event_t *event, int offset_x, int offset_y);

ui_menu_item_t *ui_menu_item_new() {
    ui_menu_item_t *mi = egoverlay_calloc(1, sizeof(ui_menu_item_t));
    mi->element.draw                = &ui_menu_item_draw;
    mi->element.process_mouse_event = &ui_menu_item_process_mouse_event;
    mi->element.get_preferred_size  = &ui_menu_item_get_preferred_size;
    mi->element.free                = &ui_menu_item_free;

    mi->pre_size = 20;
    mi->post_size = 20;
    mi->enabled = 1;

    ui_element_ref(mi);

    return mi;
}

void ui_menu_item_draw(ui_menu_item_t *item, int offset_x, int offset_y, mat4f_t *proj) {
    if (!item->child || !item->child->get_preferred_size) return;
    
    if (item->child) {
        item->child->get_preferred_size(item->child, &item->child->width, &item->child->height);
        item->child->width = item->element.width - 10 - item->pre_size - item->post_size;
    }
    if (item->pre) item->pre->get_preferred_size(item->pre, &item->pre->width, &item->pre->height);

    if (item->enabled && item->draw_bg) {
        ui_color_t bg_color;
        settings_t *app_settings = app_get_settings();
        settings_get_int(app_settings, "overlay.ui.colors.menuItemHover", (int*)&bg_color);
        ui_rect_draw(offset_x + item->element.x, offset_y + item->element.y,
                     item->element.width, item->element.height, bg_color, proj);
    }

    ui_add_input_element(offset_x, offset_y, item->element.x, item->element.y,
                         item->element.width, item->element.height, &item->element);

    if (item->child) {
        ui_element_draw(item->child, offset_x + item->element.x + 5 + item->pre_size,
                        offset_y + item->element.y + 2, proj);
    }

    if (item->pre) {
        ui_element_draw(item->pre, offset_x + item->element.x + 5, offset_y + item->element.y + 2, proj);
    }

    if (item->sub_menu) {
        char *font_path;
        int font_size = 0;

        GET_APP_SETTING_STR("overlay.ui.font.path", &font_path);
        GET_APP_SETTING_INT("overlay.ui.font.size", &font_size);
        ui_font_t *post_font = ui_font_get(font_path, font_size, 900, INT_MIN, INT_MIN);
        ui_font_render_text(post_font, proj, offset_x + item->element.x + item->element.width - item->post_size,
                            offset_y + item->element.y + 2, "\u21b4", 4, 0xFFFFFFFF);
    }
}

int ui_menu_item_get_preferred_size(ui_menu_item_t *item, int *width, int *height) {
    if (!item->child || !item->child->get_preferred_size) return 0;

    int w, h;
    if (!item->child->get_preferred_size(item->child, &w, &h)) return 0;

    *width = w + 10 + item->pre_size + item->post_size; // 5 pixel padding left+right + icon/checbkox pre item
    *height = h + 4; // 10 pixel padding top+bottom;

    return 1;
}

void ui_menu_item_free(ui_menu_item_t *item) {
    if (item->child) ui_element_unref(item->child);
    if (item->pre) ui_element_unref(item->pre);
    if (item->sub_menu) ui_element_unref(item->sub_menu);
    egoverlay_free(item);
}

int ui_menu_item_process_mouse_event(ui_menu_item_t *item, ui_mouse_event_t *event, int offset_x, int offset_y) {
    if (!item->enabled) return 1;

    if (event->event==UI_MOUSE_EVENT_TYPE_ENTER) {
        item->draw_bg = 1;

        if (item->sub_menu) {
            item->sub_menu->element.x = offset_x + item->element.x + item->element.width - 10;
            item->sub_menu->element.y = offset_y + item->element.y;
            item->parent_menu->child_menu = item->sub_menu;
            item->sub_menu->parent_menu = item->parent_menu;
            item->sub_menu->child_menu = NULL;
        } else {
            item->parent_menu->child_menu = NULL;
        }

        return 0;
    } else if (event->event==UI_MOUSE_EVENT_TYPE_LEAVE) {
        item->draw_bg = 0;
        return 0;
    }

    if (event->event==UI_MOUSE_EVENT_TYPE_BTN_UP && event->button==UI_MOUSE_EVENT_BUTTON_LEFT) {
        ui_element_call_lua_event_handlers(item, "click-left");
        return 1;
    }

    return 1;
}

void ui_menu_item_set_event_callback(ui_menu_item_t *item, menu_item_event_callback *cb) {
    item->callback = cb;
}

void ui_menu_draw(ui_menu_t *menu, int offset_x, int offset_y, mat4f_t *proj) {
    int boxw = 0;
    int boxh = 0;
    ui_element_get_preferred_size(menu->box, &boxw, &boxh);

    ui_element_set_size(menu->box, boxw, boxh);
    ui_element_set_pos(menu->box, 1, 1);
    
    menu->element.width = boxw + 2;
    menu->element.height = boxh + 2;

    ui_color_t bg_color;
    ui_color_t border_color;
    settings_t *app_settings = app_get_settings();
    settings_get_int(app_settings, "overlay.ui.colors.menuBG", (int*)&bg_color);
    settings_get_int(app_settings, "overlay.ui.colors.menuBorder", (int*)&border_color);

    // background
    ui_rect_draw(offset_x + menu->element.x, offset_y + menu->element.y,
                 menu->element.width, menu->element.height, bg_color, proj);

    // border
    ui_rect_draw( // left
        offset_x + menu->element.x,
        offset_y + menu->element.y,
        1,
        menu->element.height,
        border_color,
        proj
    );
    ui_rect_draw( // right
        offset_x + menu->element.x + menu->element.width - 1,
        offset_y + menu->element.y,
        1,
        menu->element.height,
        border_color,
        proj
    );
    ui_rect_draw( // top
        offset_x + menu->element.x,
        offset_y + menu->element.y,
        menu->element.width,
        1,
        border_color,
        proj
    );
    ui_rect_draw( // bottom
        offset_x + menu->element.x,
        offset_y + menu->element.y + menu->element.height - 1,
        menu->element.width,
        1,
        border_color,
        proj
    );

    if (!menu->parent_menu) {
        ui_add_input_element(offset_x, offset_y, menu->element.x, menu->element.y,
                             menu->element.width, menu->element.height, (ui_element_t*)menu);
    }

    ui_element_draw(menu->box, offset_x + menu->element.x, offset_y + menu->element.y, proj);

    if (menu->child_menu) ui_element_draw(menu->child_menu, 0, 0, proj);
}

ui_menu_t *ui_menu_new() {
    ui_menu_t *menu = egoverlay_calloc(1, sizeof(ui_menu_t));
    menu->element.draw = &ui_menu_draw;
    menu->element.process_mouse_event = &ui_menu_process_mouse_event;
    menu->element.free = &ui_menu_free;

    menu->box = ui_box_new(UI_BOX_ORIENTATION_VERTICAL);

    ui_element_ref(menu);

    return menu;
}

void ui_menu_free(ui_menu_t *menu) {
    ui_element_unref(menu->box);

    egoverlay_free(menu);
}

int ui_menu_process_mouse_event(ui_menu_t *menu, ui_mouse_event_t *event, int offset_x, int offset_y) {   
    //int coffx = menu->element.x + offset_x;
    //int coffy = menu->element.y + offset_y;
    int over_menu = MOUSE_EVENT_OVER_ELEMENT(event, offset_x, offset_y, menu->element) ? 1 : 0;

    // This gets complicated due to child/sub menus.
    // When a menu is shown it captures mouse input so that it receives all
    // events first. 
    // If a menu returns 0 from this function during that captured event, the
    // event will be sent to everything else that would normally get it, IE the
    // menu items, etc.
    // Therefore, the only time this function should really do anything is when
    // there is a mouse click outside of the menu or any child menu. Since a
    // child menu can also have a child menu, and so on, this can't be done
    // without recursion.

    // First, if this is a child menu and the mouse down event is over it, return 1.
    // This won't actually stop the event from reaching menu items because of the next if statements.
    if (menu->parent_menu && over_menu && event->event==UI_MOUSE_EVENT_TYPE_BTN_DOWN) {
        return 1;
    }

    // if this is a child menu that also has a child menu and the event was not
    // within this menu, see if it was within a child menu
    if (menu->parent_menu && menu->child_menu && event->event==UI_MOUSE_EVENT_TYPE_BTN_DOWN) {
        return menu->child_menu->element.process_mouse_event(menu->child_menu, event, offset_x, offset_y);
    }
   
    // If this is the top parent menu and one of the child menus returned 1 
    // above, return 0. This stops the click from being interpretted as
    // 'outside' of the menu and lets it continue on to the menu items, because
    // it was within one of the child menus.
    if (
        event->event==UI_MOUSE_EVENT_TYPE_BTN_DOWN &&
        !menu->parent_menu &&
        menu->child_menu &&
        menu->child_menu->element.process_mouse_event(menu->child_menu, event, offset_x, offset_y)
    ) return 0;
    
    // At this point the click must have been outside of this menu or any child
    // menus. Close the menu.
    if (event->event==UI_MOUSE_EVENT_TYPE_BTN_DOWN && !over_menu && !menu->parent_menu) { // && !over_child_menu) {
        ui_menu_hide(menu);
        return 0;
    }

    return 0;
}

void ui_menu_add_item(ui_menu_t *menu, ui_menu_item_t *item) {
    item->parent_menu = menu;
    ui_box_pack_end(menu->box, (ui_element_t*)item, 0, -999);
}

void ui_menu_item_set_sub_menu(ui_menu_item_t *item, ui_menu_t *menu) {
    item->sub_menu = menu;
}

void ui_menu_show(ui_menu_t *menu) {
    ui_add_top_level_element(menu);
    ui_capture_mouse_events((ui_element_t*)menu, 0, 0);
    menu->child_menu = NULL;
}

void ui_menu_hide(ui_menu_t *menu) {
    if (menu->parent_menu) ui_menu_hide(menu->parent_menu);
    else {
        ui_release_mouse_events((ui_element_t*)menu);
        ui_remove_top_level_element(menu);
    }    
}

/*** RST
Menus
=====

.. lua:currentmodule:: eg-overlay-ui

The API used to create and control context/pop-up menus is detailed below.

Functions
---------
*/

int ui_menu_lua_new(lua_State *L);
int ui_menu_lua_del(lua_State *L);
int ui_menu_lua_add_item(lua_State *L);
int ui_menu_lua_show(lua_State *L);
int ui_menu_lua_hide(lua_State *L);

int ui_menu_item_lua_new(lua_State *L);
int ui_menu_item_lua_del(lua_State *L);
int ui_menu_item_lua_set_child(lua_State *L);
int ui_menu_item_lua_enabled(lua_State *L);
int ui_menu_item_lua_set_pre(lua_State *L);
int ui_menu_item_lua_set_submenu(lua_State *L);

void ui_menu_lua_register_ui_funcs(lua_State *L) {
    lua_pushcfunction(L, &ui_menu_lua_new);
    lua_setfield(L, -2, "menu");

    lua_pushcfunction(L, &ui_menu_item_lua_new);
    lua_setfield(L, -2, "menu_item");
}

ui_menu_item_t *lua_checkuimenuitem(lua_State *L, int ind) {
    return *(ui_menu_item_t**)luaL_checkudata(L, ind, "UIMenuItemMetaTable");
}

luaL_Reg menu_funcs[] = {
    "__gc",     &ui_menu_lua_del,
    "add_item", &ui_menu_lua_add_item,
    "show",     &ui_menu_lua_show,
    "hide",     &ui_menu_lua_hide,
    NULL,    NULL
};


void lua_pushuimenu(lua_State *L, ui_menu_t *menu) {
    ui_menu_t **pmenu = lua_newuserdata(L, sizeof(ui_menu_t*));
    *pmenu = menu;

    if (luaL_newmetatable(L, "UIMenuMetaTable")) {
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
        
        lua_pushboolean(L, 1);
        lua_setfield(L, -2, "__is_uielement");

        luaL_setfuncs(L, menu_funcs, 0);
    }

    lua_setmetatable(L, -2);

    ui_element_ref(menu);
}

luaL_Reg ui_menu_item_funcs[] = {
    "__gc"              , &ui_menu_item_lua_del,
    "set_child"         , &ui_menu_item_lua_set_child,
    "set_pre"           , &ui_menu_item_lua_set_pre,
    "set_submenu"       , &ui_menu_item_lua_set_submenu,
    "enabled"           , &ui_menu_item_lua_enabled,
    "addeventhandler"   , &ui_element_lua_addeventhandler,
    "removeeventhandler", &ui_element_lua_removeeventhandler,
    NULL,           NULL
};

void lua_pushuimenuitem(lua_State *L, ui_menu_item_t *mi) {
    ui_menu_item_t **pmi = (ui_menu_item_t**)lua_newuserdata(L, sizeof(ui_menu_item_t*));
    *pmi = mi;

    if (luaL_newmetatable(L, "UIMenuItemMetaTable")) {
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");

        lua_pushboolean(L, 1);
        lua_setfield(L, -2, "__is_uielement");

        luaL_setfuncs(L, ui_menu_item_funcs, 0);
    }

    lua_setmetatable(L, -2);
    ui_element_ref(mi);
}

ui_menu_t *lua_checkuimenu(lua_State *L, int ind) {
    return *(ui_menu_t**)luaL_checkudata(L, ind, "UIMenuMetaTable");
}

/*** RST
.. lua:function:: menu()

    Create a new :lua:class:`uimenu`

    :rtype: uimenu

    .. versionhistory::
        :0.0.1: Added
*/
int ui_menu_lua_new(lua_State *L) {
    ui_menu_t *menu = ui_menu_new();
    lua_pushuimenu(L, menu);
    ui_element_unref(menu);

    return 1;
}

/*** RST
.. lua:function:: menu_item()

    Create a new :lua:class:`uimenuitem`

    :rtype: uimenuitem

    .. versionhistory::
        :0.0.1: Added
*/
int ui_menu_item_lua_new(lua_State *L) {
    ui_menu_item_t *mi = ui_menu_item_new();
    lua_pushuimenuitem(L, mi);
    ui_element_unref(mi);

    return 1;
}

/*** RST
Classes
-------

.. lua:class:: uimenu

    A pop-up menu. The only children a popup menu can have are :lua:class:`uimenuitem`
*/

int ui_menu_lua_del(lua_State *L) {
    ui_menu_t *menu = lua_checkuimenu(L, 1);

    ui_element_unref(menu);

    return 0;
}

/*** RST
    .. lua:method:: add_item(menuitem)

        Add a :lua:class:`uimenuitem` to this menu.

        :param uimenuitem menuitem:

        .. versionhistory::
            :0.0.1: Added
*/
int ui_menu_lua_add_item(lua_State *L) {
    ui_menu_t *menu = lua_checkuimenu(L, 1);

    // TODO: shouldn't this only accept menuitems??
    if (!lua_isuserdata(L, 2)) return luaL_error(L, "menu:add_item argument #1 must be a UI element.");

    ui_menu_item_t *item = lua_checkuimenuitem(L, 2);

    ui_menu_add_item(menu, item);

    return 0;
}

/*** RST
    .. lua:method:: show(x, y)

        Show the menu at the given coordinates. Typically this will be the mouse
        position. See :lua:func:`eg-overlay-ui.mouse_position`

        :param integer x:
        :param integer y:

        .. versionhistory::
            :0.0.1: Added
*/
int ui_menu_lua_show(lua_State *L) {
    ui_menu_t *menu = lua_checkuimenu(L, 1);
    int x = (int)luaL_checkinteger(L, 2);
    int y = (int)luaL_checkinteger(L, 3);

    ui_element_set_pos(menu, x, y);
    ui_menu_show(menu);

    return 0;
}

/*** RST
    .. lua:method:: hide()

        Hide this menu. If this menu is a child of another menu that menu will
        also be hidden.

        .. versionhistory::
            :0.0.1: Added
*/
int ui_menu_lua_hide(lua_State *L) {
    ui_menu_t *menu = lua_checkuimenu(L, 1);

    ui_menu_hide(menu);

    return 0;
}

/*** RST
.. lua:class:: uimenuitem

    A menu item. A menu item can only be added to a :lua:class:`uimenu`
*/

int ui_menu_item_lua_del(lua_State *L) {
    ui_menu_item_t *mi = lua_checkuimenuitem(L, 1);

    ui_element_unref(mi);
    return 0;
}

/*** RST
    .. lua:method:: set_child(element)

        Set the child of this menu item. Any UI element can be used. This can
        also be a layout container to use multiple elements.

        :param uielement element:

        .. versionhistory::
            :0.0.1: Added
*/
int ui_menu_item_lua_set_child(lua_State *L) {
    ui_menu_item_t *mi = lua_checkuimenuitem(L, 1);

    if (!lua_isuserdata(L, 2)) return luaL_error(L, "menu_item:set_child argument #1 must be a UI element.");

    ui_element_t *child = *(ui_element_t**)lua_touserdata(L, 2);
    if (mi->child) ui_element_unref(mi->child);
    mi->child = child;
    ui_element_ref(child);

    return 0;
}

/*** RST
    .. lua:method:: set_pre(element)
    
        Set the 'pre' element, which is shown to the left of the child element.
        This should be a single UI element and is intended to add elements such
        as a checkbox or icon to a menu item.

        :param uielement element:

        .. versionhistory::
            :0.0.1: Added
*/
int ui_menu_item_lua_set_pre(lua_State *L) {
    ui_menu_item_t *mi = lua_checkuimenuitem(L, 1);

    if (!lua_isuserdata(L, 2)) return luaL_error(L, "menu_item:set_pre argument #1 must be a UI element.");

    ui_element_t *pre = *(ui_element_t**)lua_touserdata(L, 2);
    if (mi->pre) ui_element_unref(mi->pre);
    mi->pre = pre;
    ui_element_ref(pre);

    return 0;
}

/*** RST
    .. lua:method:: set_submenu(menu)

        Set a menu to be shown when this menu item has the mouse over it.

        :param uimenu menu:

        .. versionhistory::
            :0.0.1: Added
*/
int ui_menu_item_lua_set_submenu(lua_State *L) {
    ui_menu_item_t *mi = lua_checkuimenuitem(L, 1);
    ui_menu_t *submenu = lua_checkuimenu(L, 2);

    if (mi->sub_menu) ui_element_unref(mi->sub_menu);
    mi->sub_menu = submenu;
    ui_element_ref(submenu);

    return 0;    
}

/*** RST
    .. lua:method:: enabled([value])

        Get or set wether this menu item is enabled or not. A disabled menu item
        does not react to mouse hover or click events.

        :param boolean value: (Optional)If present, sets if the menu item is
            enabled or not. If omitted, returns the current value.

        :rtype: boolean

        .. versionhistory::
            :0.0.1: Added
*/
int ui_menu_item_lua_enabled(lua_State *L) {
    ui_menu_item_t *mi = lua_checkuimenuitem(L, 1);

    if (lua_gettop(L)==2) {
        int enabled = lua_toboolean(L, 2);
        mi->enabled = enabled;
        return 0;
    } else {
        lua_pushboolean(L, mi->enabled);
        return 1;
    }
}

/*** RST
    .. include:: /docs/_include/ui_element_eventhandlers.rst
*/
