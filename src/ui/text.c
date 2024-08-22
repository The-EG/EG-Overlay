#include "text.h"
#include "../logging/logger.h"
#include "ui.h"
#include "../lamath.h"
#include "../utils.h"
#include <glad/gl.h>
#include <string.h>

#include <lauxlib.h>

struct ui_text_s {
    // ui_element_t
    ui_element_t element;
    // ui_text_t
    char *text;
    ui_font_t *font;
    int lines;

    ui_color_t color;

    int pref_width;
    int pref_height;

    int wrap_indices_count;
    int *wrap_indices;
};

void ui_text_free(ui_text_t *text);
static void ui_text_draw(ui_text_t *text, int offset_x, int offset_y, mat4f_t *proj);
static int ui_text_get_preferred_size(ui_text_t *text, int *width, int *height);

void ui_text_update_size(ui_text_t *text);

ui_text_t *ui_text_new(const char *text, ui_color_t color, ui_font_t *font) {
    ui_text_t *t = calloc(1, sizeof(ui_text_t));

    t->element.draw = &ui_text_draw;
    t->element.get_preferred_size = &ui_text_get_preferred_size;
    //t->element.size_updated = &ui_text_update_size; // just call the update size function anytime the size is changed
    t->element.free = &ui_text_free;

    t->color = color;
    t->font = font;

    ui_text_update_text(t, text);

    ui_element_ref(t);

    return t;
}

void ui_text_free(ui_text_t *text) {
    free(text->text);
    free(text);
}

void ui_text_update_size(ui_text_t *text) {

    text->lines = 1;
    int loffset = 0;
    text->pref_width = 0;
    for (int c=0;c<strlen(text->text);c++) {
        if (text->text[c]=='\n') {
            text->lines++;
            int width = ui_font_get_text_width(text->font, text->text + loffset, c - loffset);
            if (width > text->pref_width) text->pref_width = width;
            loffset = c + 1;
        }
    }
    int width = ui_font_get_text_width(text->font, text->text + loffset, (int)strlen(text->text) - loffset);
    if (width > text->pref_width) text->pref_width = width;

    //text->pref_width = ui_font_get_text_width(text->font, text->text, (int)strlen(text->text));
    //text->pref_height = ui_font_get_text_height(text->font);
    text->pref_height = (ui_font_get_line_spacing(text->font) * text->lines) + 2;

    if (text->wrap_indices) free(text->wrap_indices);
    text->wrap_indices_count = 0;

    /*
    if (text->element.width > 0 && text->pref_width > text->element.width) {
        text->wrap_indices_count = ui_font_get_text_wrap_indices(text->font, text->text, text->element.width, &text->wrap_indices);

        text->pref_height = ui_font_get_line_spacing(text->font) * (text->wrap_indices_count + 1);
        text->pref_width = text->element.width;
    }
    */
}


void ui_text_update_text(ui_text_t *text, const char *new_text) {
    if (text->text) free(text->text);

    size_t text_len = 0;
    for (int c=0;c<strlen(new_text);c++) {
        if (new_text[c] == '\t') text_len += 4;
        else text_len++;
    }

    text->text = calloc(text_len+1, sizeof(char));
    size_t ti = 0;
    for (int c=0;new_text[c];c++) {
        if (new_text[c]=='\t') {
            text->text[ti++] = ' ';
            text->text[ti++] = ' ';
            text->text[ti++] = ' ';
            text->text[ti++] = ' ';
        } else {
            text->text[ti++] = new_text[c];
        }
    }

    ui_text_update_size(text);
}

static int ui_text_get_preferred_size(ui_text_t *text, int *width, int *height) {
    *width = text->pref_width;
    *height = text->pref_height;
    
    return 1;
}

void ui_text_draw(ui_text_t *text, int offset_x, int offset_y,  mat4f_t *proj) {
    int x = text->element.x + offset_x;
    int y = text->element.y + offset_y + 1;

    glEnable(GL_SCISSOR_TEST);
    int old_scissor[4] = {0};
    if (push_scissor(x, y, text->element.width, text->element.height, old_scissor)) {

        int loffset = 0;
        for (int curline=0;curline<text->lines;curline++) {
            int nextl = 0;
            while ((loffset + nextl) < strlen(text->text) && text->text[loffset + nextl]!='\n') nextl++;
            ui_font_render_text(text->font, proj, x, y, text->text + loffset, nextl, text->color);
            loffset += nextl + 1;
            y += ui_font_get_line_spacing(text->font);
        }
        //if (text->wrap_indices_count==0)
        //    ui_font_render_text(text->font, proj, x, y, text->text, strlen(text->text), text->color);
        //else {
            // int wi = 0;
            // int char_ind = 0;
            // int line_len = 0;
            // int line_gap = ui_font_get_line_spacing(text->font);
            // while (wi <text->wrap_indices_count) {
            //     line_len = text->wrap_indices[wi] - char_ind;
            //     char *line_str = text->text + char_ind;
            //     ui_font_render_text(text->font, proj, x, y, line_str, line_len, text->color);
            //     char_ind = text->wrap_indices[wi] + 1;
            //     wi++;
            //     y += line_gap;
            // }

        // remainder of the string
        //ui_font_render_text(text->font, proj, x, y, text->text + char_ind, strlen(text->text) - char_ind, text->color);
        pop_scissor(old_scissor);
    }
}

void ui_text_set_pos(ui_text_t *text, int x, int y) {
    text->element.x = x;
    text->element.y = y;
}

void ui_text_set_size(ui_text_t *text, int width, int height) {
    if (width > 0) text->element.width = width;
    if (height > 0) text->element.height = height;
}


static int ui_text_lua_new(lua_State *L);

static int ui_text_lua_del(lua_State *L);
static int ui_text_lua_update_text(lua_State *L);
static int ui_text_lua_draw(lua_State *L);

static luaL_Reg ui_text_funcs[] = {
    "update_text", &ui_text_lua_update_text,
    "draw",        &ui_text_lua_draw,
    "__gc",        &ui_text_lua_del,
    NULL,           NULL
};

static void ui_text_lua_register_metatable(lua_State *L);

void ui_text_lua_register_ui_funcs(lua_State *L) {
    lua_pushcfunction(L, &ui_text_lua_new);
    lua_setfield(L, -2, "text");
}

static void ui_text_lua_register_metatable(lua_State *L) {
    if (luaL_newmetatable(L, "UITextMetaTable")) {
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");

        luaL_setfuncs(L, ui_text_funcs, 0);
    }
}

void lua_push_ui_text(lua_State *L, ui_text_t *text) {
    ui_element_ref(text);

    ui_text_t **t = lua_newuserdata(L, sizeof(ui_text_t*));
    *t = text;

    ui_text_lua_register_metatable(L);
    lua_setmetatable(L, -2);
}

/*** RST
Text Elements
=============

.. lua:currentmodule:: overlay-ui

Functions
---------

.. lua:function:: text(text2, color, fontpath, fontsize[, fontweight[, fontslant[, fontwidth]]])

    Create a new :lua:class:`uitext` element.

    .. note::
        See :ref:`fonts` for more detail on the font* parameters.

    :param text: The text string to display.
    :type text: string
    :param color: Text color. This is an integer value in RGBA format. See :ref:`colors`.
    :type color: integer
    :param fontpath: The path to the font to use.
    :type fontpath: string
    :param fontsize: The font size, expressed as a pixel height.
    :type fontsize: integer
    :param fontweight: (Optional) The font weight.
    :type fontweight: integer
    :param fontslant: (Optional) The font slant.
    :type fontslant: integer
    :param fontwidth: (Optional) the font width.
    :type fontwidth: integer
    :return: A text UI element

    .. versionhistory::
        :0.0.1: Added
*/
static int ui_text_lua_new(lua_State *L) {
    const char *text = luaL_checkstring(L, 1);
    ui_color_t color = (ui_color_t)luaL_checkinteger(L, 2);
    const char *font_path = luaL_checkstring(L, 3);
    int font_size = (int)luaL_checkinteger(L, 4);

    int weight = INT_MIN;
    int slant = INT_MIN;
    int width = INT_MIN;

    if (lua_gettop(L)>=5) weight = (int)luaL_checkinteger(L, 5);
    if (lua_gettop(L)>=6) slant  = (int)luaL_checkinteger(L, 6);
    if (lua_gettop(L)>=7) width  = (int)luaL_checkinteger(L, 7);

    ui_font_t *font = ui_font_get(font_path, font_size, weight, slant, width);

    if (!font) return luaL_error(L, "Couldn't load font %s.", font_path);

    ui_text_t *t = ui_text_new(text, color, font);

    lua_push_ui_text(L, t);
    ui_element_unref(t);
    
    return 1;
}

static int ui_text_lua_del(lua_State *L) {
    ui_text_t *text = *(ui_text_t**)luaL_checkudata(L, 1, "UITextMetaTable");

    ui_element_unref(text);

    return 0;
}

/*** RST
Classes
-------

.. lua:class:: uitext

    A text element.

    .. lua:method:: update_text(newtext)

        Update the text displayed by this text element. It will be automatically
        resized.

        :param newtext: The new text to display.
        :type newtext: string

        .. versionhistory::
            :0.0.1: Added
*/
static int ui_text_lua_update_text(lua_State *L) {
    ui_text_t *text = *(ui_text_t**)luaL_checkudata(L, 1, "UITextMetaTable");
    const char *new_text = luaL_checkstring(L, 2);

    ui_text_update_text(text, new_text);

    return 0;
}


static int ui_text_lua_draw(lua_State *L) {
    ui_text_t *text = *(ui_text_t**)luaL_checkudata(L, 1, "UITextMetaTable");
    int x = (int)luaL_checkinteger(L, 2);
    int y = (int)luaL_checkinteger(L, 3);
    mat4f_t *proj = mat4f_from_lua(L, 4);

    ui_text_draw(text, x, y, proj);

    return 0;
}
