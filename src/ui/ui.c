#include <windows.h>
#include "ui.h"
#include "logging/logger.h"
#include "utils.h"
#include "lamath.h"
#include "rect.h"
#include "font.h"
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
#include "image.h"
#include "../utils.h"
#include "../dx.h"

#include <lauxlib.h>

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

typedef struct {
    int cbi;
    char *event;
} ui_element_event_handler_data_t;

int ui_element_lua_event_handler_callback(lua_State *L, ui_element_event_handler_data_t *data);

void ui_init() {
    ui = egoverlay_calloc(1, sizeof(ui_t));

    ui->log = logger_get("ui");

    logger_debug(ui->log, "init");

    // default style
    settings_t *app_settings = app_get_settings();
    settings_set_default_int(app_settings, "overlay.ui.colors.windowBG",              0x000000bb);
    settings_set_default_int(app_settings, "overlay.ui.colors.windowBorder",          0x3D4478FF);
    settings_set_default_int(app_settings, "overlay.ui.colors.windowBorderHighlight", 0x3d5a78ff);

    settings_set_default_int(app_settings, "overlay.ui.colors.text",                  0xFFFFFFFF);
    settings_set_default_int(app_settings, "overlay.ui.colors.accentText",            0xFCBA03FF);

    settings_set_default_int(app_settings, "overlay.ui.colors.buttonBG",              0x1F253BDD);
    settings_set_default_int(app_settings, "overlay.ui.colors.buttonBGHover",         0x2E3859FF);
    settings_set_default_int(app_settings, "overlay.ui.colors.buttonBGHighlight",     0x3a4670FF);
    settings_set_default_int(app_settings, "overlay.ui.colors.buttonBorder",          0x3D4478FF);

    settings_set_default_int(app_settings, "overlay.ui.colors.menuBG",                0x161a26DD);
    settings_set_default_int(app_settings, "overlay.ui.colors.menuBorder",            0x3D4478FF);
    settings_set_default_int(app_settings, "overlay.ui.colors.menuItemHover",         0x2E3859FF);
    settings_set_default_int(app_settings, "overlay.ui.colors.menuItemHighlight",     0x3a4670FF);

    settings_set_default_string(app_settings, "overlay.ui.font.path"    , "fonts/Inter.ttf");
    settings_set_default_string(app_settings, "overlay.ui.font.pathMono", "fonts/CascadiaCode.ttf");
    settings_set_default_string(app_settings, "overlay.ui.font.pathIcon", "fonts/MaterialSymbolsOutlined.ttf");
    settings_set_default_int   (app_settings, "overlay.ui.font.size"    , 12);
    settings_set_default_int   (app_settings, "overlay.ui.font.weight"  , 400);
    settings_set_default_int   (app_settings, "overlay.ui.font.slant"   , 0);

    ui_rect_init();
    ui_font_init();
    ui_image_init();

    ui->input_mutex = CreateMutex(0, FALSE, NULL);

    lua_manager_add_module_opener("eg-overlay-ui", &ui_lua_open_module);
}

void ui_clear_top_level_elements() {
    ui_element_list_t *tle=ui->top_level_elements;
    while (tle) {
        ui_element_list_t *n = tle->next;
        ui_element_unref(tle->element);
        egoverlay_free(tle);
        tle = n;
    }
    ui->top_level_elements = NULL;
}

void ui_cleanup() {
    
    ui_input_element_t *e = ui->input_elements;
    while (e) {
        ui_input_element_t *prev = e->prev;
        egoverlay_free(e);
        e = prev;
    }

    CloseHandle(ui->input_mutex);
    logger_debug(ui->log, "cleanup");
    ui_image_cleanup();
    ui_rect_cleanup();
    ui_font_cleanup();

    egoverlay_free(ui);
}

void ui_element_draw(void *element, int offset_x, int offset_y, mat4f_t *proj) {
    ui_element_t *e = (ui_element_t*)element;

    if (!e->draw) return;

    if (!dx_region_visible(offset_x + e->x, offset_y + e->y, offset_x + e->x + e->width, offset_y + e->y + e->height)) return;

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


    e->draw(element, offset_x, offset_y, proj);
}

int ui_element_has_focus(ui_element_t *element) {
    return ui->focus_element==element ? 1 : 0;
}

void ui_grab_focus(void *element) {
    if (ui->focus_element) ui_element_call_lua_event_handlers(ui->focus_element, "unfocus");
    ui->focus_element = element;
    ui_element_call_lua_event_handlers(element, "focus");
}

void ui_clear_focus() {
    if (ui->focus_element) ui_element_call_lua_event_handlers(ui->focus_element, "unfocus");
    ui->focus_element = NULL;
}

void ui_add_top_level_element(void *element) {
    for (ui_element_list_t *e=ui->top_level_elements;e;e=e->next) {
        
        if (e->element==element) {
            logger_debug(ui->log, "Element 0x%x already a top level element.", element);
            return;
        }
    }

    ui_element_list_t *e = egoverlay_calloc(1, sizeof(ui_element_list_t));

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

            egoverlay_free(e);
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
        egoverlay_free(n);
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

    if (element->ref_count) return;

    WaitForSingleObject(ui->input_mutex, INFINITE);

    if (ui->mouse_over_element == element) {
        ui->mouse_over_element = NULL;
    }

    ui_input_element_t *e = ui->input_elements;
    ui_input_element_t *next = NULL;
    while (e) {
        ui_input_element_t *prev = e->prev;
        if (e->element==element) {
            if (ui->input_elements==e) ui->input_elements = prev;
            egoverlay_free(e);
            if (next) next->prev = prev;
            break;
        } else {
            next = e;
        }
        e = prev;
    }

    ReleaseMutex(ui->input_mutex);

    for (size_t hi=0;hi<element->lua_event_handler_count;hi++) {
        lua_manager_unref(element->lua_event_handlers[hi]);
    }
    if (element->lua_event_handlers) egoverlay_free(element->lua_event_handlers);

    if (element->free) element->free(element);

    }

void ui_add_input_element(int offset_x, int offset_y, int x, int y, int w, int h, ui_element_t *element) {
    ui_input_element_t *e = egoverlay_calloc(1, sizeof(ui_input_element_t));

    e->offset_x = offset_x;
    e->offset_y = offset_y;
    e->x = x;
    e->y = y;
    e->w = w;
    e->h = h;

    e->element = element;

    WaitForSingleObject(ui->input_mutex, INFINITE);

    e->prev = ui->input_elements;
    ui->input_elements = e;

    ReleaseMutex(ui->input_mutex);
}

static void ui_send_leave_event(ui_element_t *element, int offset_x, int offset_y) {
    ui_mouse_event_t leave = {0};
    leave.event = UI_MOUSE_EVENT_TYPE_LEAVE;
    if (element->process_mouse_event) element->process_mouse_event(element, &leave, offset_x, offset_y);
    ui_element_call_lua_event_handlers(element, "leave"); 
}

static void ui_send_enter_event(ui_element_t *element, int offset_x, int offset_y) {
    ui_mouse_event_t enter = {0};
    enter.event = UI_MOUSE_EVENT_TYPE_ENTER;
    if (element->process_mouse_event) element->process_mouse_event(element, &enter, offset_x, offset_y);
    ui_element_call_lua_event_handlers(element, "enter");
}

void ui_send_lua_mouse_event(ui_element_t *element, ui_mouse_event_t *event) {
    const char *ename = "unknown";

    if (event->event==UI_MOUSE_EVENT_TYPE_BTN_DOWN) {
        if      (event->button==UI_MOUSE_EVENT_BUTTON_LEFT  ) ename = "btn-down-left";
        else if (event->button==UI_MOUSE_EVENT_BUTTON_RIGHT ) ename = "btn-down-right";
        else if (event->button==UI_MOUSE_EVENT_BUTTON_MIDDLE) ename = "btn-down-middle";
        else if (event->button==UI_MOUSE_EVENT_BUTTON_X1    ) ename = "btn-down-x1";
        else if (event->button==UI_MOUSE_EVENT_BUTTON_X2    ) ename = "btn-down-x2";
        else                                                  ename = "btn-down-unk";
    } else if (event->event==UI_MOUSE_EVENT_TYPE_BTN_UP) {
        if      (event->button==UI_MOUSE_EVENT_BUTTON_LEFT  ) ename = "btn-up-left";
        else if (event->button==UI_MOUSE_EVENT_BUTTON_RIGHT ) ename = "btn-up-right";
        else if (event->button==UI_MOUSE_EVENT_BUTTON_MIDDLE) ename = "btn-up-middle";
        else if (event->button==UI_MOUSE_EVENT_BUTTON_X1    ) ename = "btn-up-x1";
        else if (event->button==UI_MOUSE_EVENT_BUTTON_X2    ) ename = "btn-up-x2";
        else                                                  ename = "btn-up-unk";
    } else if (event->event==UI_MOUSE_EVENT_TYPE_MOVE) {
        ename = "move";
    } else if (event->event==UI_MOUSE_EVENT_TYPE_WHEEL) {
        if (event->value<=0) ename = "wheel-up";
        else                 ename = "wheel-down";
    } else if (event->event==UI_MOUSE_EVENT_TYPE_HWHEEL) {
        if (event->value<=0) ename = "wheel-left";
        else                 ename = "wheel-right";
    } else if (event->event==UI_MOUSE_EVENT_TYPE_ENTER) ename = "enter";
      else if (event->event==UI_MOUSE_EVENT_TYPE_LEAVE) ename = "leave";

    ui_element_call_lua_event_handlers(element, ename);
}

int ui_process_mouse_event(ui_mouse_event_t *event) {
    WaitForSingleObject(ui->input_mutex, INFINITE);

    ui->last_mouse_x = event->x;
    ui->last_mouse_y = event->y;

    ui_input_element_t *top_element_under_mouse = NULL;

        for (ui_input_element_t *e = ui->input_elements;e;e = e->prev) {
        if (MOUSE_POINT_IN_RECT(event->x, event->y, e->offset_x + e->x, e->offset_y + e->y, e->w, e->h)) {
            top_element_under_mouse = e;
            break;
        }
    }

    if (top_element_under_mouse && ui->mouse_over_element!=top_element_under_mouse->element) {
        if (ui->mouse_over_element) {
            // proper offsets shouldn't be needed for leave/enter
            ui_send_leave_event(ui->mouse_over_element, 0, 0);
        }
        ui_send_enter_event(
            top_element_under_mouse->element,
            top_element_under_mouse->offset_x,
            top_element_under_mouse->offset_y
        );
        ui->mouse_over_element = top_element_under_mouse->element;
    } else if (top_element_under_mouse==NULL && ui->mouse_over_element) {
        ui_send_leave_event(ui->mouse_over_element, 0, 0);
        ui->mouse_over_element = NULL;
    }

    if (ui->mouse_capture_element) {
        ui_send_lua_mouse_event(ui->mouse_capture_element, event);

        if (ui->mouse_capture_element->process_mouse_event(
                ui->mouse_capture_element,
                event,
                ui->capture_offset_x,
                ui->capture_offset_y
            )) {
            ReleaseMutex(ui->input_mutex);
            return 1;
        }
    }

    if (top_element_under_mouse) {
        ui_send_lua_mouse_event(top_element_under_mouse->element, event);
    }

    ui_input_element_t *e = ui->input_elements;

    while (e) { 
        if (MOUSE_POINT_IN_RECT(event->x, event->y, e->offset_x + e->x, e->offset_y + e->y, e->w, e->h)) {
            if (e->element->process_mouse_event) {
                if (e->element->process_mouse_event(e->element, event, e->offset_x, e->offset_y)) {
                    ReleaseMutex(ui->input_mutex);
                    return 1;
                }
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

int ui_element_lua_addeventhandler(lua_State *L) {
    ui_element_t *element = lua_checkuielement(L, 1);

    if (!lua_isfunction(L, 2)) return luaL_error(L, "Event handler must be a function.");

    int *h = element->lua_event_handlers;
    size_t newcount = element->lua_event_handler_count + 1;
    
    element->lua_event_handlers = egoverlay_realloc(h, newcount * sizeof(int));

    lua_pushvalue(L, 2);
    int cbi = luaL_ref(L, LUA_REGISTRYINDEX);
    element->lua_event_handlers[element->lua_event_handler_count++] = cbi;

    lua_pushinteger(L, cbi);
    
    return 1;
}


int ui_element_lua_removeeventhandler(lua_State *L) {
    ui_element_t *element = lua_checkuielement(L, 1);
    int cbi = (int)luaL_checkinteger(L, 2);

    size_t cbi_ind = 0;
    for (size_t i=0;i<element->lua_event_handler_count;i++) {
        if (element->lua_event_handlers[i]==cbi) {
            cbi_ind = i;
            break;
        }
    }
    
    if (cbi_ind==0 && element->lua_event_handlers[cbi_ind]!=cbi) {
        return luaL_error(L, "Lua event handler not found.");
    }

    for (size_t i=cbi_ind;i<element->lua_event_handler_count-1;i++) {
        element->lua_event_handlers[i] = element->lua_event_handlers[i+1];
    }

    element->lua_event_handlers = egoverlay_realloc(
        element->lua_event_handlers,
        sizeof(int) * (element->lua_event_handler_count - 1)
    );
    element->lua_event_handler_count--;

    return 0;
}

void ui_element_call_lua_event_handlers(ui_element_t *element, const char *event) {
    for (size_t hi=0;hi<element->lua_event_handler_count;hi++) {
        ui_element_event_handler_data_t *d = egoverlay_calloc(1, sizeof(ui_element_event_handler_data_t));

        d->cbi = element->lua_event_handlers[hi];
        d->event = egoverlay_calloc(strlen(event)+1, sizeof(char));
        memcpy(d->event, event, strlen(event));

        lua_manager_add_event_callback(&ui_element_lua_event_handler_callback, d);
    }
}

int ui_element_lua_event_handler_callback(lua_State *L, ui_element_event_handler_data_t *data) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, data->cbi);
    lua_pushstring(L, data->event);

    egoverlay_free(data->event);
    egoverlay_free(data);
    
    return 1;
}

int ui_element_lua_background(lua_State *L) {
    ui_element_t *element = lua_checkuielement(L, 1);

    if (lua_gettop(L)==2) {
        element->bg_color = (ui_color_t)luaL_checkinteger(L, 2);
        return 0;
    }

    lua_pushinteger(L, element->bg_color);

    return 1;
}

static int ui_lua_element(lua_State *L);
static void ui_lua_element_draw(ui_lua_element_t *element, int offset_x, int offset_y,  mat4f_t *proj);
static int ui_lua_add_top_level_element(lua_State *L);
static int ui_lua_remove_top_level_element(lua_State *L);

static int ui_lua_mouse_position(lua_State *L);
static int ui_lua_mouse_button_state(lua_State *L);

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
    image
*/

static const struct luaL_Reg ui_funcs[] = {
    "addtoplevelelement"   , &ui_lua_add_top_level_element,
    "removetoplevelelement", &ui_lua_add_top_level_element,
    "mouseposition"        , &ui_lua_mouse_position,
    "mousebuttonstate"     , &ui_lua_mouse_button_state,
    NULL                   ,  NULL
};

int ui_lua_open_module(lua_State *L) {
    lua_newtable(L);
    luaL_setfuncs(L, ui_funcs, 0);

    ui_text_lua_register_ui_funcs(L);
    ui_window_lua_register_ui_funcs(L);
    ui_button_lua_register_ui_funcs(L);
    ui_box_lua_register_ui_funcs(L);
    ui_scroll_view_register_lua_funcs(L);
    ui_text_entry_lua_register_funcs(L);
    ui_menu_lua_register_ui_funcs(L);
    ui_separator_lua_register_ui_funcs(L);
    ui_grid_lua_register_ui_funcs(L);
    ui_image_lua_register_ui_funcs(L);

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
    performance, glyphs are pre-rendered to a texture array that is then used to
    render glyphs each frame.

    Unlike many other UI frameworks, this pre-rendering is not static. Each font
    face, size, weight, slant, and width combination has a 512x512xN texture
    array, where N is a variable number of layers that are created as needed.
    When a unique font combination is requested a texture array with a single
    layer is created and pre-rendered with the standard ASCII glyphs. 

    Additional glyphs are pre-rendered the first time they are requested. This
    means that the UI may suffer a performance hit on the first frame a new
    glyph is rendered, but all subsequent frames should be unaffected.

.. _ui-events:

UI Events
---------

Most UI elements can send events to event handlers in Lua. Such elements
implement ``addeventhandler`` and ``removeeventhandler`` methods. Some elements
do not emit events by default and need to be enabled using an ``events`` method.

Event handler functions are passed a single argument, a string describing the
event:

=================== ======================================
Value               Description
=================== ======================================
``btn-down-left``   Mouse button 1 (left) pressed.
``btn-down-right``  Mouse button 2 (right) pressed.
``btn-down-middle`` Mouse button 3 (middle) pressed.
``btn-down-x1``     Mouse button 4 pressed.
``btn-down-x2``     Mouse button 5 pressed.
``btn-down-unk``    An unknown mouse button is pressed.
``btn-up-left``     Mouse button 1 (left) released.
``btn-up-right``    Mouse button 2 (right) released.
``btn-up-middle``   Mouse button 3 (middle) released.
``btn-up-x1``       Mouse button 4 released.
``btn-up-x2``       Mouse button 5 released.
``btn-up-unk``      An unknown mouse button is released.
``move``            Mouse cursor moved (over the element).
``wheel-up``        Mouse wheel scrolled up.
``wheel-down``      Mouse wheel scrolled down.
``wheel-left``      Mouse wheel scrolled left.
``wheel-right``     Mouse wheel scrolled right.
``enter``           Mouse cursor entered the element area.
``leave``           Mouse cursor left the element area.
``focus``           Element now has focus (keyboard input).
``unfocus``         Element no longer has focus.
=================== ======================================

.. note::

    Elements may send additional events not listed here.

.. important::

    Events are sent to Lua like any other event, asynchronously. This means that
    there may be some delay between an event occurring and the event handler
    being called in Lua. However, events still should arrive in the same order
    that they occurred.

Core UI
-------

.. lua:class:: uielement

    This is not an actual class accessible from Lua, but represents any UI
    Element.

    .. versionhistory::
        :0.0.1: Added

.. lua:function:: addtoplevelelement(uielement)

    Add an element to the list of top level elements. Top level elements are
    the only elements drawn automatically.

    .. note::
        Normally only windows are top level elements, however any UI element can be
        specified here to have it drawn independently of a window.

    If ``uielement`` is already a top level element this function has no effect.

    :param uielement: The UI element to draw.
    
    .. versionhistory::
        :0.0.1: Added
        :0.1.0: Renamed from add_top_level_element to addtoplevelelement
*/
int ui_lua_add_top_level_element(lua_State *L) {
    ui_element_t *element = *(ui_element_t**)lua_touserdata(L, 1);    

    ui_add_top_level_element(element);

    return 0;
}

/*** RST
.. lua:function:: removetoplevelelement(uielement)

    Remove a top level element previously added with
    :lua:func:`addtoplevelelement`.

    :param uielement: The UI element to hide.

    .. versionhistory::
        :0.0.1: Added
        :0.1.0: Renamed from remove_top_level_element to removetoplevelelement
*/
int ui_lua_remove_top_level_element(lua_State *L) {
    ui_element_t *element = *(ui_element_t**)lua_touserdata(L, 1);

    ui_remove_top_level_element(element);

    return 0;
}

/*** RST
.. lua:function:: mouseposition()

    Returns the x,y coordinates of the mouse cursor. These coordinates are
    relative to the overlay's client rectangle, i.e. 0,0 is the top left.

    :return: X,Y
    :rtype: integer

    .. code-block:: lua
        :caption: Example

        local x,y = ui.mouse_position()
    
    .. versionhistory::
        :0.0.1: Added
        :0.1.0: Renamed from mouse_position to mouseposition
*/
static int ui_lua_mouse_position(lua_State *L) {
    lua_pushinteger(L, ui->last_mouse_x);
    lua_pushinteger(L, ui->last_mouse_y);

    return 2;
}

/*** RST
.. lua:function:: mousebuttonstate()

    Returns the state of the left, middle, and right mouse buttons.

    This function returns 3 boolean values, indicating if the left, middle, and
    right mouse button are pressed (true) or not (false).

    :rtype: boolean

    .. versionhistory::
        :0.1.0: Added
*/
static int ui_lua_mouse_button_state(lua_State *L) {
    int swaplr = GetSystemMetrics(SM_SWAPBUTTON);

    lua_pushboolean(L, GetAsyncKeyState(swaplr ? VK_RBUTTON : VK_LBUTTON) & (1 << 15));
    lua_pushboolean(L, GetAsyncKeyState(                      VK_MBUTTON) & (1 << 15));
    lua_pushboolean(L, GetAsyncKeyState(swaplr ? VK_LBUTTON : VK_RBUTTON) & (1 << 15));

    return 3;
}

int lua_checkuialign(lua_State *L, int ind) {
    const char *align_str = luaL_checkstring(L, ind);

    if      (strcmp(align_str, "start" )==0) return   -1;
    else if (strcmp(align_str, "middle")==0) return    0;
    else if (strcmp(align_str, "end"   )==0) return    1;
    else if (strcmp(align_str, "fill"  )==0) return -999;
    
    return luaL_error(L, "align argument must be one of: "
                         "'start', 'middle', 'end', or 'fill'.");
}

ui_element_t *lua_checkuielement(lua_State *L, int ind) {
    if (lua_type(L, ind)!=LUA_TUSERDATA) luaL_error(L, "Argument #%d is not a UI element.", ind);
    ui_element_t *e = *(ui_element_t**)lua_touserdata(L, ind);

    if (e==NULL) luaL_error(L, "Argument #%d is not a UI element.", ind);

    if (lua_getmetatable(L, ind)==0) {
        luaL_error(L, "Argument #%d is not a UI element.", ind);
    }

    if (lua_getfield(L, ind, "__is_uielement")!=LUA_TBOOLEAN) {
        luaL_error(L, "Argument #%d is not a UI element.", ind);
    }

    if (!lua_isboolean(L, -1)) {
        luaL_error(L, "Argument #%d is not a UI element.", ind);
    }

    int is_uielement = lua_toboolean(L, -1);
    lua_pop(L, 2); // __is_uielement and the metatable

    if (!is_uielement) {
        luaL_error(L, "Argument #%d is not a UI element.", ind);
    }

    return e;
}
