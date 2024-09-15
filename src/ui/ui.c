#include <windows.h>
#include "ui.h"
#include "logging/helpers.h"
#include "utils.h"
#include "lamath.h"
#include "rect.h"
#include "font.h"
#include "polyline.h"
#include "image.h"
#include "window.h"
#include "menu.h"
#include "button.h"
#include "text.h"
#include "box.h"
#include "scrollview.h"
#include "lua-manager.h"
#include "app.h"
#include "text-entry.h"
#include "separator.h"
#include "grid.h"

#include <lauxlib.h>

struct ui_lua_element_t {
    ui_element_t element;

    lua_State *lua;
    int draw_cbi;
};

typedef struct ui_input_element_t {
    int offset_x;
    int offset_y;
    int x;
    int y;
    int w;
    int h;
    ui_element_t *element;

    struct ui_input_element_t *prev;
} ui_input_element_t;

typedef struct {
    logger_t *log;

    //ui_element_list_t *elements;

    ui_element_list_t *top_level_elements;

    ui_element_t *mouse_capture_element;
    int capture_offset_x;
    int capture_offset_y;

    int last_mouse_x;
    int last_mouse_y;

    ui_element_t *mouse_over_element;

    ui_input_element_t *input_elements;

    HANDLE input_mutex;

    ui_element_t *focus_element;
} ui_t;

static ui_t *ui = NULL;

int ui_lua_open_module(lua_State *L);

void ui_init() {
    ui = calloc(1, sizeof(ui_t));

    ui->log = logger_get("ui");

    logger_debug(ui->log, "init");

    // default style
    settings_t *app_settings = app_get_settings();
    settings_set_default_int(app_settings, "overlay.ui.colors.windowBG",              0x000000bb);
    settings_set_default_int(app_settings, "overlay.ui.colors.windowBorder",          0x3D4478FF);
    settings_set_default_int(app_settings, "overlay.ui.colors.windowBorderHighlight", 0x3d5a78ff);

    settings_set_default_int(app_settings, "overlay.ui.colors.text",                  0xFFFFFFFF);

    settings_set_default_int(app_settings, "overlay.ui.colors.buttonBG",              0x1F253BDD);
    settings_set_default_int(app_settings, "overlay.ui.colors.buttonBGHover",         0x2E3859FF);
    settings_set_default_int(app_settings, "overlay.ui.colors.buttonBGHighlight",     0x3a4670FF);
    settings_set_default_int(app_settings, "overlay.ui.colors.buttonBorder",          0x3D4478FF);

    //settings_set_default_int(app_settings, "overlay.ui.colors.menuBG",                0x1F253BDD);
    settings_set_default_int(app_settings, "overlay.ui.colors.menuBG",                0x161a26DD);
    settings_set_default_int(app_settings, "overlay.ui.colors.menuBorder",            0x3D4478FF);
    settings_set_default_int(app_settings, "overlay.ui.colors.menuItemHover",         0x2E3859FF);
    settings_set_default_int(app_settings, "overlay.ui.colors.menuItemHighlight",     0x3a4670FF);

    settings_set_default_string(app_settings, "overlay.ui.font.path"    , "fonts/Inter.ttf");
    settings_set_default_string(app_settings, "overlay.ui.font.pathMono", "fonts/CascadiaCode.ttf");
    settings_set_default_int   (app_settings, "overlay.ui.font.size"    , 12);
    settings_set_default_int   (app_settings, "overlay.ui.font.weight"  , 400);
    settings_set_default_int   (app_settings, "overlay.ui.font.slant"   , 0);

    ui_rect_init();
    ui_font_init();
    ui_polyline_init();
    ui_image_init();

    ui->input_mutex = CreateMutex(0, FALSE, NULL);

    lua_manager_add_module_opener("eg-overlay-ui", &ui_lua_open_module);
}

void ui_cleanup() {
    ui_input_element_t *e = ui->input_elements;
    while (e) {
        ui_input_element_t *prev = e->prev;
        free(e);
        e = prev;
    }

    CloseHandle(ui->input_mutex);
    logger_debug(ui->log, "cleanup");
    ui_polyline_cleanup();
    ui_rect_cleanup();
    ui_font_cleanup();
    ui_image_cleanup();

    free(ui);
}

void ui_element_draw(void *element, int offset_x, int offset_y, mat4f_t *proj) {
    ui_element_t *e = (ui_element_t*)element;

    if (UI_COLOR_A_INT(e->bg_color)>0) {
        int bgx = offset_x + e->x;
        int bgy = offset_y + e->y;
        ui_rect_draw(bgx, bgy, e->width, e->height, e->bg_color, proj);
    }

    if (UI_COLOR_A_INT(e->border_color)>0) {
        // left
        ui_rect_draw(offset_x + e->x, offset_y + e->y, 1, e->height, e->border_color, proj);
        // top
        ui_rect_draw(offset_x + e->x + 1, offset_y + e->y, e->width - 2, 1, e->border_color, proj);
        // right
        ui_rect_draw(offset_x + e->x + e->width - 1, offset_y + e->y, 1, e->height, e->border_color, proj);
        // bottom
        ui_rect_draw(offset_x + e->x + 1, offset_y + e->y + e->height - 1, e->width - 2, 1, e->border_color, proj);
    }

    if (!e->draw) return;

    e->draw(element, offset_x, offset_y, proj);
}

int ui_element_has_focus(ui_element_t *element) {
    return ui->focus_element==element ? 1 : 0;
}

void ui_grab_focus(void *element) {
    ui->focus_element = element;
}

void ui_clear_focus() {
    ui->focus_element = NULL;
}

void ui_add_top_level_element(void *element) {
    for (ui_element_list_t *e=ui->top_level_elements;e;e=e->next) {
        
        if (e->element==element) {
            logger_debug(ui->log, "Element 0x%x already a top level element.", element);
            return;
        }
    }

    ui_element_list_t *e = calloc(1, sizeof(ui_element_list_t));

    e->element = element;

    ui_element_ref(element);

    if (!ui->top_level_elements) {
        ui->top_level_elements = e;
        return;
    }

    ui_element_list_t *last = ui->top_level_elements;
    while (last->next) last = last->next;
    last->next = e;
    e->prev = last;
}

void ui_remove_top_level_element(void *element) {
    ui_element_list_t *e = ui->top_level_elements;

    ui_element_unref(element);

    while (e) {
        if (e->element == element) {
            if (e->prev) {
                e->prev->next = e->next;
            }
            if (e->next) {
                e->next->prev = e->prev;
            }

            if (ui->top_level_elements == e) {
                ui->top_level_elements = e->next;
            }

            free(e);
            return;
        }
        e = e->next;
    }
}

void ui_move_element_to_top(void *element) {
    ui_element_list_t *e = ui->top_level_elements;

    if (e->element==element && !e->next) return; // this is the only element being drawn

    while (e) {
        if (e->element==element) {
            if (!e->next) return; // already on top

            ui_element_list_t *last = e->next;
            while (last->next) {
                last = last->next;
            }

            if (e->prev) e->prev->next = e->next;
            e->next->prev = e->prev;
            if (ui->top_level_elements==e) ui->top_level_elements = e->next;

            last->next = e;
            e->prev = last;
            e->next = NULL;
            return;
        }
        e = e->next;
    }
}

void ui_draw(mat4f_t *proj) {
    WaitForSingleObject(ui->input_mutex, INFINITE);
    // clear out the input element list. it will be populated as elements draw each frame
    ui_input_element_t *ie = ui->input_elements;
    while (ie) {
        ui_input_element_t *n = ie;
        ie = ie->prev;
        free(n);
    }
    ui->input_elements = NULL;

    ui_element_list_t *e = ui->top_level_elements;
    while(e) {
        ui_element_draw(e->element, 0, 0, proj);
        e = e->next;
    }

    ReleaseMutex(ui->input_mutex);
}

void ui_element_set_size(ui_element_t *element, int width, int height) {
    element->width = width;
    element->height = height;

    if (element->size_updated) element->size_updated(element);
}

void ui_element_set_pos(ui_element_t *element, int x, int y) {
    element->x = x;
    element->y = y;
}

int ui_element_get_preferred_size(ui_element_t *element, int *width, int *height) {
    if (!element || !element->get_preferred_size) return 0;
    
    int w;
    int h;
    if (element->get_preferred_size(element, &w, &h)) {
        *width = w;
        *height = h;
        return 1;
    }

    return 0;
}

void ui_element_ref(ui_element_t *element) {
    element->ref_count++;
}

void ui_element_unref(ui_element_t *element) {
    element->ref_count--;

    if (element->ref_count==0 && element->free) element->free(element);
}

void ui_add_input_element(int offset_x, int offset_y, int x, int y, int w, int h, ui_element_t *element) {
    ui_input_element_t *e = calloc(1, sizeof(ui_input_element_t));

    e->offset_x = offset_x;
    e->offset_y = offset_y;
    e->x = x;
    e->y = y;
    e->w = w;
    e->h = h;

    e->element = element;

    e->prev = ui->input_elements;
    ui->input_elements = e;
}

static void ui_send_leave_event(ui_element_t *element, int offset_x, int offset_y) {
    ui_mouse_event_t leave = {0};
    leave.event = UI_MOUSE_EVENT_TYPE_LEAVE;
    if (element->process_mouse_event) element->process_mouse_event(element, &leave, offset_x, offset_y);
}

static void ui_send_enter_event(ui_element_t *element, int offset_x, int offset_y) {
    ui_mouse_event_t enter = {0};
    enter.event = UI_MOUSE_EVENT_TYPE_ENTER;
    if (element->process_mouse_event) element->process_mouse_event(element, &enter, offset_x, offset_y);
}

int ui_process_mouse_event(ui_mouse_event_t *event) {
    ui->last_mouse_x = event->x;
    ui->last_mouse_y = event->y;

    if (ui->mouse_capture_element) {
        // still need to give the enter and leave events to the element that has captured
        // the mouse, so do that before sending the actual event
        int mouse_over_capture = MOUSE_POINT_IN_RECT(
            event->x,
            event->y,
            ui->capture_offset_x + ui->mouse_capture_element->x,
            ui->capture_offset_y + ui->mouse_capture_element->y,
            ui->mouse_capture_element->width,
            ui->mouse_capture_element->height
        ) ? 1 : 0;

        if (ui->mouse_over_element==ui->mouse_capture_element && !mouse_over_capture) {
            ui_send_leave_event(ui->mouse_capture_element, ui->capture_offset_x, ui->capture_offset_y);
            ui->mouse_over_element = NULL;
        } else if (ui->mouse_over_element==NULL && mouse_over_capture) {
            ui_send_enter_event(ui->mouse_capture_element, ui->capture_offset_x, ui->capture_offset_y);
            ui->mouse_over_element = ui->mouse_capture_element;
        }
        if (ui->mouse_capture_element->process_mouse_event(
                ui->mouse_capture_element,
                event,
                ui->capture_offset_x,
                ui->capture_offset_y
            )) return 1;
    }

    WaitForSingleObject(ui->input_mutex, INFINITE);
    ui_input_element_t *e = ui->input_elements;

    while (e) {
        if (MOUSE_POINT_IN_RECT(event->x, event->y, e->offset_x + e->x, e->offset_y + e->y, e->w, e->h)) {
            if (ui->mouse_over_element!=e->element) {
                if (ui->mouse_over_element) ui_send_leave_event(ui->mouse_over_element, e->offset_x, e->offset_y);
                ui->mouse_over_element = e->element;
                ui_send_enter_event(e->element, e->offset_x, e->offset_y);
            }
        } else if (ui->mouse_over_element==e->element) {
            if (ui->mouse_over_element) ui_send_leave_event(ui->mouse_over_element, e->offset_x, e->offset_y);
                ui->mouse_over_element = NULL;
        }
        if (e->element->process_mouse_event &&
            (MOUSE_POINT_IN_RECT(event->x, event->y, e->offset_x + e->x, e->offset_y + e->y, e->w, e->h))) {
            if (e->element->process_mouse_event(e->element, event, e->offset_x, e->offset_y)) {
                ReleaseMutex(ui->input_mutex);
                return 1;
            }
        }
        e = e->prev;
    }

    ReleaseMutex(ui->input_mutex);

    if (event->event==UI_MOUSE_EVENT_TYPE_BTN_DOWN) ui->focus_element = NULL;

    return 0;
}

int ui_process_keyboard_event(ui_keyboard_event_t *event) {
    if (ui->focus_element && ui->focus_element->process_keyboard_event) {
        return ui->focus_element->process_keyboard_event(ui->focus_element, event);
    }

    return 0;
}

void ui_capture_mouse_events(ui_element_t *element, int offset_x, int offset_y) {
    if (ui->mouse_capture_element==NULL && element->process_mouse_event) {
        ui->mouse_capture_element = element;
        ui->capture_offset_x = offset_x;
        ui->capture_offset_y = offset_y;
    }
}

void ui_release_mouse_events(ui_element_t *element) {
    if (ui->mouse_capture_element==element) {
        ui->mouse_capture_element = NULL;
        ui->capture_offset_x = 0;
        ui->capture_offset_y = 0;
    }
}

static int ui_lua_element(lua_State *L);
static void ui_lua_element_draw(ui_lua_element_t *element, int offset_x, int offset_y,  mat4f_t *proj);
static int ui_lua_add_top_level_element(lua_State *L);
static int ui_lua_remove_top_level_element(lua_State *L);

static int ui_lua_mouse_position(lua_State *L);

/*** RST
eg-overlay-ui
=============

.. lua:module:: eg-overlay-ui

.. code-block:: lua

    local ui = require 'eg-overlay-ui'

.. toctree::
    :caption: Top Level Elements
    :maxdepth: 1

    window

.. toctree::
    :caption: Layout Containers
    :maxdepth: 1

    box
    grid

.. toctree::
    :caption: UI Elements
    :maxdepth: 1

    text
    button
    scrollview
    separator
    text-entry
    menu
*/

static const struct luaL_Reg ui_funcs[] = {
    //"element",        &ui_lua_element,
    "add_top_level_element",    &ui_lua_add_top_level_element,
    "remove_top_level_element", &ui_lua_add_top_level_element,
    "mouse_position",           &ui_lua_mouse_position,
    NULL,              NULL
};

int ui_lua_open_module(lua_State *L) {
    lua_newtable(L);
    luaL_setfuncs(L, ui_funcs, 0);

    ui_text_lua_register_ui_funcs(L);
    ui_rect_lua_register_ui_funcs(L);
    ui_window_lua_register_ui_funcs(L);
    ui_polyline_lua_register_ui_funcs(L);
    ui_image_lua_register_ui_funcs(L);
    ui_button_lua_register_ui_funcs(L);
    ui_box_lua_register_ui_funcs(L);
    ui_scroll_view_register_lua_funcs(L);
    ui_text_entry_lua_register_funcs(L);
    ui_menu_lua_register_ui_funcs(L);
    ui_separator_lua_register_ui_funcs(L);
    ui_grid_lua_register_ui_funcs(L);

    return 1;
}

/*** RST
.. _colors:

Colors
------

All colors in EG-Overlay are represented by 32bit integers in RGBA format. This
may sound complicated, but it is actually incredibly convenient for module
authors as colors can be specified in hex format, similar to CSS. For example,
red at 100% opacity is ``0xFF0000FF``, green is ``0x00FF00FF``,
and blue is ``0x0000FFFF``.

.. _fonts:

Fonts
-----
.. important::
    EG-Overlay uses FreeType2 to render glyphs onscreen. This means that any font
    that FreeType2 supports can be used, **however module authors are encouraged to
    use the fonts configured in the overlay settings**.

Due to the rendering and caching system, fonts are not directly exposed to Lua.
Instead, all UI elements that use fonts take the following arguments on creation:

- Font path
- Font size
- Font weight
- Font slant
- Font width

The font path is the path to the actual font file. This can be a static/single
font file or a variable font containing multiple styles. If a font does not
support the given style parameters (weight, slant, width) they will be ignored.
If style parameters are omitted, defaults will be used.

.. admonition:: Implementation Detail

    Fonts are rendered using a textured quad for each glyph. To attain useable
    performance, glyphs are pre-rendered to a texture that is then used to
    render glyphs each frame.

    Unlike many other UI frameworks, this pre-rendering is not static. Each font
    face, size, weight, slant, and width combination has a set of 512x512
    texture 'pages' where glyphs are pre-rendered. When a unique font combination
    is requested a new page is created and pre-rendered with the standard ASCII
    glyphs. 

    Additional glyphs are pre-rendered the first time they are requested. This
    means that the UI may suffer a performance hit on the first frame a new
    glyph is rendered, but all subsequent frames should be unaffected.

    Due to the fact that the texture pages are a fixed size (512x512), larger
    font sizes will require more pages. Additional pages will have a small
    negative impact on render performance since each time a glyph is rendered,
    each page must be searched for it in sequence. Since all ASCII glyphs are
    pre-rendered first, this should put the most common glyphs on the first page
    and most commonly used font sizes should have extra room on the first page
    after that. In reality, this should only be a concern for huge font sizes
    (> 40).

Core UI
-------

.. lua:class:: uielement

    This is not an actual class accessible from Lua, but represents any UI
    Element.

    .. versionhistory::
        :0.0.1: Added

.. lua:function:: add_top_level_element(uielement)

    Add an element to the list of top level elements. Top level elements are
    the only elements drawn automatically.

    .. note::
        Normally only windows are top level elements, however any UI element can be
        specified here to have it drawn independently of a window.

    If ``uielement`` is already a top level element this function has no effect.

    :param uielement: The UI element to draw.
    
    .. versionhistory::
        :0.0.1: Added
*/
int ui_lua_add_top_level_element(lua_State *L) {
    ui_element_t *element = *(ui_element_t**)lua_touserdata(L, 1);    

    ui_add_top_level_element(element);

    return 0;
}

/*** RST
.. lua:function:: remove_top_level_element(uielement)

    Remove a top level element previously added with
    :lua:func:`add_top_level_element`.

    :param uielement: The UI element to hide.

    .. versionhistory::
        :0.0.1: Added
*/
int ui_lua_remove_top_level_element(lua_State *L) {
    ui_element_t *element = *(ui_element_t**)lua_touserdata(L, 1);

    ui_remove_top_level_element(element);

    return 0;
}

/*** RST
.. lua:function:: mouse_position()

    Returns the x,y coordinates of the mouse cursor. These coordinates are
    relative to the overlay's client rectangle, i.e. 0,0 is the top left.

    :return: X,Y
    :rtype: integer

    .. code-block:: lua
        :caption: Example

        local x,y = ui.mouse_position()
    
    .. versionhistory::
        :0.0.1: Added
*/
static int ui_lua_mouse_position(lua_State *L) {
    lua_pushinteger(L, ui->last_mouse_x);
    lua_pushinteger(L, ui->last_mouse_y);

    return 2;
}

static int ui_lua_element_set_draw(lua_State *L);
static int ui_lua_element_del(lua_State *L);

static luaL_Reg ui_lua_element_funcs[] = {
    "draw", &ui_lua_element_set_draw,
    "__gc", &ui_lua_element_del,
    NULL,    NULL
};

static void ui_lua_element_register_metatable(lua_State *L) {
    if (luaL_newmetatable(L, "UILuaElementMetaTable")) {
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");

        luaL_setfuncs(L, ui_lua_element_funcs, 0);
    }
}

static int ui_lua_element(lua_State *L) {
    ui_lua_element_t **element = lua_newuserdata(L, sizeof(ui_lua_element_t*));
    ui_lua_element_t *e = calloc(1, sizeof(ui_lua_element_t));

    *element = e;

    e->element.draw = &ui_lua_element_draw;
    e->lua = L;
    e->draw_cbi = -1;

    ui_lua_element_register_metatable(L);
    lua_setmetatable(L, -2);

    return 1;
}

void ui_lua_element_draw(ui_lua_element_t *element, int offset_x, int offset_y, mat4f_t *proj) {
    UNUSED_PARAM(offset_x);
    UNUSED_PARAM(offset_y);

    if (element->draw_cbi > 0) {
        lua_rawgeti(element->lua, LUA_REGISTRYINDEX, element->draw_cbi);
        mat4f_push_to_lua(proj, element->lua);
        if (lua_pcall(element->lua, 1, 0, 0)!=LUA_OK) {
            const char *errmsg = luaL_checkstring(element->lua, -1);
            logger_error(ui->log, "Error occured during lua element draw: %s", errmsg);
            lua_pop(element->lua, 1);
        }
    }
}

static int ui_lua_element_set_draw(lua_State *L) {
    ui_lua_element_t *element = *(ui_lua_element_t**)luaL_checkudata(L, 1, "UILuaElementMetaTable");
    if (element->draw_cbi>0) luaL_unref(L, LUA_REGISTRYINDEX, element->draw_cbi);
    lua_pushvalue(L, -1);
    element->draw_cbi = luaL_ref(L, LUA_REGISTRYINDEX);

    return 0;
}

static int ui_lua_element_del(lua_State *L) {
    ui_lua_element_t *element = *(ui_lua_element_t**)luaL_checkudata(L, 1, "UILuaElementMetaTable");

    if (element->draw_cbi>0) luaL_unref(L, LUA_REGISTRYINDEX, element->draw_cbi);

    free(element);

    return 0;
}


int ui_lua_check_align(lua_State *L, int ind) {
    const char *align_str = luaL_checkstring(L, ind);

    if      (strcmp(align_str, "start" )==0) return   -1;
    else if (strcmp(align_str, "middle")==0) return    0;
    else if (strcmp(align_str, "end"   )==0) return    1;
    else if (strcmp(align_str, "fill"  )==0) return -999;
    
    return luaL_error(L, "align argument must be one of: "
                         "'start', 'middle', 'end', or 'fill'.");
}

ui_element_t *lua_checkuielement(lua_State *L, int ind) {
    ui_element_t *e = *(ui_element_t**)lua_touserdata(L, ind);

    if (e==NULL) return luaL_error(L, "Argument #%d is not a UI element.", ind);

    if (lua_getmetatable(L, ind)==0) {
        return luaL_error(L, "Argument #%d is not a UI element.", ind);
    }

    if (lua_getfield(L, ind, "__is_uielement")!=LUA_TBOOLEAN) {
        return luaL_error(L, "Argument #%d is not a UI element.", ind);
    }

    if (!lua_isboolean(L, -1)) {
        return luaL_error(L, "Argument #%d is not a UI element.", ind);
    }

    int is_uielement = lua_toboolean(L, -1);
    lua_pop(L, 2); // __is_uielement and the metatable

    if (!is_uielement) {
        return luaL_error(L, "Argument #%d is not a UI element.", ind);
    }

    return e;
}
