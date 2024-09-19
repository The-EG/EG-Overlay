#pragma once
#include "color.h"
#include <stdint.h>
#include "../lamath.h"
#include <lua.h>

typedef struct ui_element_t ui_element_t;
typedef struct ui_font_t ui_font_t;

typedef enum {
    UI_MOUSE_EVENT_TYPE_BTN_DOWN,
    UI_MOUSE_EVENT_TYPE_BTN_UP,
    UI_MOUSE_EVENT_TYPE_MOVE,
    UI_MOUSE_EVENT_TYPE_WHEEL,
    UI_MOUSE_EVENT_TYPE_HWHEEL,
    UI_MOUSE_EVENT_TYPE_ENTER,
    UI_MOUSE_EVENT_TYPE_LEAVE
} ui_mouse_event_type;

typedef enum {
    UI_MOUSE_EVENT_BUTTON_LEFT,
    UI_MOUSE_EVENT_BUTTON_RIGHT,
    UI_MOUSE_EVENT_BUTTON_MIDDLE,
    UI_MOUSE_EVENT_BUTTON_X1,
    UI_MOUSE_EVENT_BUTTON_X2
} ui_mouse_event_button;

typedef struct {
    long x;
    long y;
    ui_mouse_event_type event;
    ui_mouse_event_button button;
    int value;
} ui_mouse_event_t;

#define MOUSE_POINT_IN_RECT(x, y, rx, ry, rw, rh) ( \
    x >= rx && \
    x <= rx + rw && \
    y >= ry && \
    y <= ry + rh \
)
#define MOUSE_EVENT_OVER_ELEMENT(event, offsetx, offsety, element) \
    MOUSE_POINT_IN_RECT( \
        event->x, \
        event->y, \
        offsetx + element.x, \
        offsety + element.y, \
        element.width, \
        element.height \
    )
#define MOUSE_EVENT_OVER_ELEMENT_P(event, offsetx, offsety, element) \
    MOUSE_POINT_IN_RECT( \
        event->x, \
        event->y, \
        offsetx + element->x, \
        offsety + element->y, \
        element->width, \
        element->height \
    )
#define MOUSE_EVENT_IS_LEFT_UP(e) \
    (e->event==UI_MOUSE_EVENT_TYPE_BTN_UP && e->button==UI_MOUSE_EVENT_BUTTON_LEFT)
#define MOUSE_EVENT_IS_LEFT_DN(e) \
    (e->event==UI_MOUSE_EVENT_TYPE_BTN_DOWN && e->button==UI_MOUSE_EVENT_BUTTON_LEFT)

typedef enum {
    UI_KEYBOARD_EVENT_TYPE_KEYDOWN,
    UI_KEYBOARD_EVENT_TYPE_KEYUP
} ui_keyboard_event_type;

typedef struct {
    uint32_t vk_key;
    int down;
    int alt;
    int shift;
    int caps;
    int ctrl;
    char ascii[3];
} ui_keyboard_event_t;

int ui_process_keyboard_event(ui_keyboard_event_t *event);

void ui_element_draw(void *element, int offset_x, int offset_y, mat4f_t *proj);
void ui_element_set_size(void *element, int width, int height);
void ui_element_set_pos(void *element, int x, int y);
int ui_element_get_preferred_size(void *element, int *width, int *height);

void ui_add_input_element(int offset_x, int offset_y, int x, int y, int w, int h, ui_element_t *element);

typedef struct ui_element_list_t {
    ui_element_t *element;
    struct ui_element_list_t *next;
    struct ui_element_list_t *prev;
} ui_element_list_t;

typedef void ui_draw_fn(void *element, int offset_x, int offset_y, mat4f_t *proj);
typedef int ui_process_mouse_event_fn(void *element, ui_mouse_event_t *me, int offset_x, int offset_y);
typedef int ui_process_keyboard_event_fn(void *element, ui_keyboard_event_t *event);
typedef int ui_get_preferred_size_fn(void *element, int *width, int *height);
typedef int ui_size_updated_fn(void *element);
typedef void ui_element_free_fn(void *element);

struct ui_element_t {
    ui_draw_fn *draw;
    ui_process_mouse_event_fn *process_mouse_event;
    ui_process_keyboard_event_fn *process_keyboard_event;
    ui_get_preferred_size_fn *get_preferred_size;
    ui_size_updated_fn *size_updated;
    ui_element_free_fn *free;

    int x;
    int y;
    int width;
    int height;

    int min_width;
    int min_height;
    int max_width;
    int max_height;

    ui_color_t bg_color;
    ui_color_t fg_color;
    ui_color_t border_color;

    uint8_t ref_count;

    size_t lua_event_handler_count;
    int *lua_event_handlers;
};

int ui_element_has_focus(void *element);
void ui_grab_focus(void *element);
void ui_clear_focus();

void ui_element_ref(void *element);
void ui_element_unref(void *element);

// Lua functions common to most UI elements
int ui_element_lua_addeventhandler(lua_State *L);
int ui_element_lua_removeeventhandler(lua_State *L);
int ui_element_lua_background(lua_State *L);

void ui_element_call_lua_event_handlers(void *element, const char *event);

void ui_init();
void ui_cleanup();

void ui_add_top_level_element(void *element);
void ui_remove_top_level_element(void *element);
void ui_clear_top_level_elements();
void ui_move_element_to_top(void *element);

int ui_process_mouse_event(ui_mouse_event_t *event);

void ui_draw(mat4f_t *proj);

void ui_capture_mouse_events(ui_element_t *element, int offset_x, int offset_y);
void ui_release_mouse_events(ui_element_t *element);

ui_font_t *ui_default_font();

float ui_window_caption_size();

typedef struct ui_lua_element_t ui_lua_element_t;

int ui_lua_check_align(lua_State *L, int ind);

ui_element_t *lua_checkuielement(lua_State *L, int ind);
