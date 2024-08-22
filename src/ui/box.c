#include "box.h"
#include "utils.h"
#include <lauxlib.h>
#include <string.h>
#include "rect.h"

typedef struct ui_box_item_t {
    ui_element_t *item;
    struct ui_box_item_t *next;
    int expand;
    int align;
} ui_box_item_t;

struct ui_box_t {
    ui_element_t element;
    ui_box_orientation_e orientation;
    ui_box_item_t *items;

    int align;

    struct {
        int left;
        int right;
        int top;
        int bottom;
    } padding;

    int spacing;
};

static void ui_box_free(ui_box_t *box);
static void ui_box_draw(ui_box_t *box, int offset_x, int offset_y, mat4f_t *proj);
static int ui_box_get_preferred_size(ui_box_t *box, int *width, int *height);

ui_box_t *ui_box_new(ui_box_orientation_e orientation) {
    ui_box_t *box = calloc(1, sizeof(ui_box_t));
    box->element.draw = &ui_box_draw;
    box->element.get_preferred_size = &ui_box_get_preferred_size;
    box->element.free = &ui_box_free;
    box->orientation = orientation;
    box->align = -1;

    ui_element_ref(box);

    return box;
}

void ui_box_free(ui_box_t *box) {
    ui_box_item_t *i = box->items;
    while (i) {
        ui_box_item_t *n = i->next;
        ui_element_unref(i->item);
        free(i);
        i = n;
    }

    free(box);
}

void ui_box_pack_end(ui_box_t *box, ui_element_t *element, int expand, int align) {
    ui_element_ref(element);
    
    ui_box_item_t *i = calloc(1, sizeof(ui_box_item_t));
    i->item = element;
    i->align = align;
    i->expand = expand;

    if (box->items==NULL) {
        box->items = i;
        return;
    }

    ui_box_item_t *a = box->items;
    while (a->next) a = a->next;
    a->next = i;
}

void ui_box_set_padding(ui_box_t *box, int left, int right, int top, int bottom) {
    if (left>=0) box->padding.left = left;
    if (right>=0) box->padding.right = right;
    if (top>=0) box->padding.top = top;
    if (bottom>=0) box->padding.bottom = bottom;
}

static void ui_box_draw(ui_box_t *box, int offset_x, int offset_y, mat4f_t *proj) {
    int y = 0; //offset_y + box->element.y + box->padding.top;
    int x = 0; //offset_x + box->element.x + box->padding.left;

    int pref_width;
    int pref_height;
    ui_box_get_preferred_size(box, &pref_width, &pref_height);

    //if (box->element.width - box->padding.left - box->padding.right < pref_width) pref_width = box->element.width - box->padding.left - box->padding.right;
    //if (box->element.height - box->padding.top - box->padding.bottom < pref_height) pref_height = box->element.height - box->padding.top - box->padding.bottom;
    if (box->element.width < pref_width) pref_width = box->element.width;
    if (box->element.height < pref_height) pref_height = box->element.height;

    // how much extra width/height do we have to give to fill items?
    int extra_room = 0;
    if (box->orientation==UI_BOX_ORIENTATION_HORIZONTAL) {
        extra_room = box->element.width - pref_width - box->padding.left - box->padding.right;
    } else {
        extra_room = box->element.height - pref_height - box->padding.bottom - box->padding.top;
    }

    // how many items are set to fill?
    int fill_items = 0;
    for (ui_box_item_t *i = box->items; i; i = i->next) {
        if (i->expand) fill_items++;
    }

    // each fill item can expand this amount. or 0
    int item_fill_size = fill_items ? extra_room / fill_items : 0;

    // we'll need this to offset the x/y later. if no items are filled then no offsets
    if (fill_items==0) extra_room = 0;

    //ui_rect_draw(offset_x + box->element.x + box->padding.left, offset_y + box->element.y + box->padding.top, box->element.width - box->padding.left - box->padding.right, box->element.height - box->padding.top - box->padding.bottom, 0x00FF00FF, proj);

    if (box->orientation==UI_BOX_ORIENTATION_VERTICAL) {
        if      (box->align< 0) y = offset_y + box->element.y + box->padding.top;
        else if (box->align==0) y = offset_y + box->element.y + (box->element.height / 2) - ((pref_height + extra_room) / 2);
        else                    y = offset_y + box->element.y + box->element.height - box->padding.bottom - pref_height;

        ui_box_item_t *i = box->items;
        while (i) {
            int item_width = 0;
            int item_height = 0;
            if (i->item->get_preferred_size && i->item->get_preferred_size(i->item, &item_width, &item_height)) {
                if (i->expand) item_height += item_fill_size;
                i->item->width = item_width;
                i->item->height = item_height;
                //if (item_width > box->element.width) item_width = box->element.width;
                //ui_element_set_size(i->item, item_width, item_height); 
            }

            if (i->align==-999) i->item->width = box->element.width - box->padding.left - box->padding.right;

            if      (i->align<0 ) x = offset_x + box->element.x + box->padding.left;
            else if (i->align==0) x = offset_x + box->element.x + (box->element.width / 2) - (item_width / 2);
            else                  x = offset_x + box->element.x + box->element.width - box->padding.right - item_width;

            ui_element_draw(i->item, x, y, proj);
            
            y += i->item->height;
            if (i->next) y += box->spacing;
            
            i = i->next;
        }
    } else {
        if      (box->align< 0) x = offset_x + box->element.x + box->padding.left;
        else if (box->align==0) x = offset_x + box->element.x + box->padding.left + (int)((float)(box->element.width - box->padding.left - box->padding.right) / 2.f) - (int)((float)(pref_width - box->padding.left - box->padding.right) / 2.f);
        else                    x = offset_x + box->element.x + box->element.width - box->padding.right - pref_width;

        ui_box_item_t *i = box->items;
        while (i) {
            int item_width = 0;
            int item_height = 0;
            if (i->item->get_preferred_size && i->item->get_preferred_size(i->item, &item_width, &item_height)) {
                if (i->expand) item_width += item_fill_size;                
                i->item->width = item_width;                
                i->item->height = item_height;
            }

            if (i->align==-999) i->item->height = box->element.height - box->padding.top - box->padding.bottom;

            if      (i->align< 0) y = offset_y + box->element.y + box->padding.top;
            else if (i->align==0) y = offset_y + box->element.y + (box->element.height / 2) - (item_height / 2);
            else                  y = offset_y + box->element.y + box->element.height - box->padding.bottom - item_height;

            //ui_rect_draw(x, y, item_width, item_height, 0xFF0000FF, proj);

            ui_element_draw(i->item, x, y, proj);
            x += i->item->width;
            if (i->next) x += box->spacing;
            i = i->next;
        }
    }
}

static int ui_box_get_preferred_size(ui_box_t *box, int *width, int *height) {
    *height = box->padding.top + box->padding.bottom;
    *width = box->padding.left + box->padding.right;

    if (box->orientation==UI_BOX_ORIENTATION_VERTICAL) {
        int max_item_width = 0;

        ui_box_item_t *i = box->items;
        while (i) {
            int item_width;
            int item_height;

            if (i->item->get_preferred_size && i->item->get_preferred_size(i->item, &item_width, &item_height)) {
                if (max_item_width<item_width) max_item_width = item_width;
                *height += item_height;
                if (i->next) *height += box->spacing;
            }
            
            i = i->next;
        }

        *width += max_item_width;
    } else {
        int max_item_height = 0;

        ui_box_item_t *i = box->items;
        while (i) {
            int item_width;
            int item_height;

            if (i->item->get_preferred_size && i->item->get_preferred_size(i->item, &item_width, &item_height)) {
                if (max_item_height<item_height) max_item_height = item_height;
                *width += item_width;
                if (i->next) *width += box->spacing;
            }
            i = i->next;
        }

        *height += max_item_height;
    }

    return 1;    
}

static int ui_box_lua_new(lua_State *L);
static int ui_box_lua_del(lua_State *L);
static int ui_box_lua_set_padding(lua_State *L);
static int ui_box_lua_pack_end(lua_State *L);
static int ui_box_lua_set_align(lua_State *L);
static int ui_box_lua_spacing(lua_State *L);
static int ui_box_lua_item_count(lua_State *L);
static int ui_box_lua_pop_start(lua_State *L);

static luaL_Reg ui_box_funcs[] = {
    "__gc",       &ui_box_lua_del,
    "padding",    &ui_box_lua_set_padding,
    "pack_end",   &ui_box_lua_pack_end,
    "align",      &ui_box_lua_set_align,
    "spacing",    &ui_box_lua_spacing,
    "item_count", &ui_box_lua_item_count,
    "pop_start",  &ui_box_lua_pop_start,
    NULL,        NULL
};

void ui_box_lua_register_ui_funcs(lua_State *L) {
    lua_pushcfunction(L, &ui_box_lua_new);
    lua_setfield(L, -2, "box"); 
}

static void ui_box_lua_register_metatable(lua_State *L) {
    if (luaL_newmetatable(L, "UIBoxMetaTable")) {
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
        luaL_setfuncs(L, ui_box_funcs, 0);
    }
}


void lua_push_uibox(ui_box_t *box, lua_State *L) {
    ui_box_t **b = lua_newuserdata(L, sizeof(ui_box_t*));
    *b = box;

    ui_box_lua_register_metatable(L);
    lua_setmetatable(L, -2);
    ui_element_ref(box); // hold a reference to the box
}

/*** RST
Box Layouts
===========

.. lua:currentmodule:: overlay-ui

Functions
---------

.. lua:function:: box(orientation)

    Create a new box layout container. Box layout containers arrange their child
    elements sequentially in either a vertical or horizontal fashion.

    :param orientation: The layout orientation. Either ``vertical`` or ``horizontal``.
    :type orientation: string
    :return: A new :lua:class:`uibox`.

    .. versionhistory::
        :0.0.1: Added
*/
static int ui_box_lua_new(lua_State *L) {
    const char *orient = luaL_checkstring(L, 1);
    ui_box_orientation_e o;

    if (strcmp(orient, "vertical")==0) o = UI_BOX_ORIENTATION_VERTICAL;
    else if (strcmp(orient, "horizontal")==0) o = UI_BOX_ORIENTATION_HORIZONTAL;
    else {
        return luaL_error(L, "ui.box(orientation): orientation must be either 'vertical' or 'horizontal.'");
    }

    ui_box_t *box = ui_box_new(o);
    lua_push_uibox(box, L);
    ui_element_unref(box); // release our reference here, Lua still has one

    return 1;
}

#define CHECK_UI_BOX(L, ind) *(ui_box_t**)luaL_checkudata(L, ind, "UIBoxMetaTable")

static int ui_box_lua_del(lua_State *L) {
    ui_box_t *box = CHECK_UI_BOX(L, 1);

    ui_element_unref(box); // release Lua's ref, that will free it if no other refs exist

    return 0;
}

/*** RST
Classes
-------

.. lua:class:: uibox

    A box layout container. 

    Box layout containers arrange their child elements sequentially in either a
    vertical or horizontal fashion.

    .. lua:method:: padding(left, right, top, bottom)

        Set the internal padding used between the edge of the box and its child
        elements.

        :param left:
        :type left: integer
        :param right:
        :type right: integer
        :param top:
        :type top: integer
        :param bottom:
        :type bottom: integer

        .. versionhistory::
            :0.0.1: Added

*/
static int ui_box_lua_set_padding(lua_State *L) {
    ui_box_t *box = CHECK_UI_BOX(L, 1);
    int left = (int)luaL_checkinteger(L, 2);
    int right = (int)luaL_checkinteger(L, 3);
    int top = (int)luaL_checkinteger(L, 4);
    int bottom = (int)luaL_checkinteger(L, 5);

    ui_box_set_padding(box, left, right, top, bottom);

    return 0;
}

/*** RST
    .. lua:method:: pack_end(uielement[, expand[, align]])

        Add a child element to this box. It will be added after any exist elements.

        :param uielement: Child element
        :param expand: (Optional) Should the child element expand to fill any
            extra space within the box? Default: ``false``
        :type expand: boolean
        :param align: (Optional) The child's alignment within the box. Note: this
            is perpendicular to the axis of the box. In a horizontal box, this is
            the vertical alignment. Must be one of: ``'start'``, ``'middle'``,
            ``'end'``, or ``'fill'``. Default: ``'start'``
        :type align: string

        .. versionhistory::
            :0.0.1: Added
*/
static int ui_box_lua_pack_end(lua_State *L) {
    int args = lua_gettop(L);

    if (args <2 || args >4) 
    return luaL_error(L, "Invalid number of arguments to box:pack_end. box:pack_end(ui_element [, expand [, align]])");

    ui_box_t *box = CHECK_UI_BOX(L, 1);

    if (!lua_isuserdata(L, 2)) return luaL_error(L, "box:pack_end argument #1 must be a UI element.");

    ui_element_t *element = *(ui_element_t**)lua_touserdata(L, 2);

    int fill = 0;
    int align = -1;

    if (args>2) fill = lua_toboolean(L, 3);
    if (args==4) {
        align = ui_lua_check_align(L, 4);
    }

    ui_box_pack_end(box, element, fill, align);

    return 0;
}

/*** RST
    .. lua:method:: align(alignment)

        Set the alignment of all child elements within the box.

        This is along the axis of the box. For a horizontal box, ``'start'`` will
        cause child elements to be drawn to the left most portion of the box,
        ``'middle'`` will center the elements, and ``'end'`` will draw the
        child elements to the right most portion.

        :param alignment: Alignment value. Must be one of: ``'start'``,
            ``'middle'``, ``'end'``.

        .. versionhistory::
            :0.0.1: Added
*/
static int ui_box_lua_set_align(lua_State *L) {
    ui_box_t *box = CHECK_UI_BOX(L, 1);

    box->align = ui_lua_check_align(L, 2);

    return 0;
}

/*** RST
    .. lua:method:: spacing(value)
    
        Set the spacing inserted between child elements.

        :param value:
        :type value: integer

        .. versionhistory::
            :0.0.1: Added
*/
static int ui_box_lua_spacing(lua_State *L) {
    ui_box_t *box = CHECK_UI_BOX(L, 1);
    int spacing = (int)luaL_checkinteger(L, 2);

    box->spacing = spacing;

    return 0;
}

/*** RST
    .. lua:method:: item_count()

        Return the number of child elements in this box.

        :rtype: integer

        .. versionhistory::
                :0.0.1: Added
*/
static int ui_box_lua_item_count(lua_State *L) {
    ui_box_t *box = CHECK_UI_BOX(L, 1);
    int count = 0;
    for (ui_box_item_t *i=box->items;i;i=i->next) count++;

    lua_pushinteger(L, count);

    return 1;
}

/*** RST
    .. lua:method:: pop_start()

        Remove the first child element from this box.

        .. versionhistory::
            :0.0.1: Added
*/
static int ui_box_lua_pop_start(lua_State *L) {
    ui_box_t *box = CHECK_UI_BOX(L, 1);

    ui_box_item_t *f = box->items;

    if (f) {
        box->items = f->next;
        ui_element_unref(f->item);
        free(f);
    }

    return 0;
}