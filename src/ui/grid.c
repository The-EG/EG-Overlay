#include <stdlib.h>
#include <string.h>
#include <lauxlib.h>
#include "rect.h"
#include "grid.h"
#include "ui.h"
#include "../utils.h"

typedef struct ui_grid_cell_t {
    ui_element_t *item;
    int rowspan;
    int colspan;

    int halign;
    int valign;
} ui_grid_cell_t;

struct ui_grid_t {
    ui_element_t element;

    int rows;
    int cols;

    int *rowspacing;
    int *colspacing;

    int *colwidths;
    int *rowheights;

    ui_grid_cell_t *cells;
};

#define GRID_CELL(grid, row, col) grid->cells[(row * grid->cols) + col]

int ui_grid_get_preferred_size(ui_grid_t *grid, int *width, int *height);
void ui_grid_draw(ui_grid_t *grid, int offset_x, int offset_y, mat4f_t *proj);
void ui_grid_free(ui_grid_t *grid);

ui_grid_t *ui_grid_new(int rows, int cols) {
    ui_grid_t *grid = egoverlay_calloc(1, sizeof(ui_grid_t));

    grid->cells = egoverlay_calloc(rows * cols, sizeof(ui_grid_cell_t));

    grid->rows = rows;
    grid->cols = cols;

    grid->rowspacing = egoverlay_calloc(rows, sizeof(int));
    grid->rowheights = egoverlay_calloc(rows, sizeof(int));
    grid->colspacing = egoverlay_calloc(cols, sizeof(int));
    grid->colwidths = egoverlay_calloc(cols, sizeof(int));

    grid->element.draw = &ui_grid_draw;
    grid->element.free = &ui_grid_free;
    grid->element.get_preferred_size = &ui_grid_get_preferred_size;

    ui_element_ref(grid);

    return grid;
}

void ui_grid_free(ui_grid_t *grid) {
    for (int c=0;c<grid->rows*grid->cols;c++) {
        if (grid->cells[c].item) ui_element_unref(grid->cells[c].item);
    }

    egoverlay_free(grid->cells);
    egoverlay_free(grid->rowspacing);
    egoverlay_free(grid->rowheights);
    egoverlay_free(grid->colspacing);
    egoverlay_free(grid->colwidths);
    egoverlay_free(grid);
}

void ui_grid_attach(
    ui_grid_t *grid,
    void *uielement,
    int row, int col,
    int rowspan,
    int colspan,
    int horizalign,
    int vertalign
) {
    ui_grid_cell_t *cell = &GRID_CELL(grid, row, col);

    if (cell->item) ui_element_unref(cell->item);

    cell->item = uielement;
    cell->rowspan = rowspan;
    cell->colspan = colspan;
    cell->halign = horizalign;
    cell->valign = vertalign;
    ui_element_ref(cell->item);
}

void ui_grid_rowspacing(ui_grid_t *grid, int row, int spacing) {
    grid->rowspacing[row] = spacing;
}

void ui_grid_colspacing(ui_grid_t *grid, int col, int spacing) {
    grid->colspacing[col] = spacing;
}

int ui_grid_get_preferred_size(ui_grid_t *grid, int *width, int *height) {
    int pheight = 0;
    int pwidth = 0;

    memset(grid->rowheights, 0, sizeof(int) * grid->rows);
    memset(grid->colwidths, 0, sizeof(int) * grid->cols);

    for (int r=0;r<grid->rows;r++) {
        for (int c=0;c<grid->cols;c++) {
            ui_grid_cell_t *cell = &GRID_CELL(grid, r, c);
            if (!cell->item) continue;

            int cw = 0;
            int ch = 0;
            if (cell->item->get_preferred_size && cell->item->get_preferred_size(cell->item, &cw, &ch)) {
                if (grid->colwidths[c] < cw && cell->colspan==1) grid->colwidths[c] = cw;
                if (grid->rowheights[r] < ch && cell->rowspan==1) grid->rowheights[r] = ch;
                ui_element_set_size(cell->item, cw, ch);
            }
        }
        pheight += grid->rowheights[r] + grid->rowspacing[r];
    }

    for (int c=0;c<grid->cols;c++) {
        pwidth += grid->colwidths[c] + grid->colspacing[c];
    }

    // cell items were set to their preferred sizes above, but any that had
    // 'fill' alignments need to be fixed up now
    for (int r=0;r<grid->rows;r++) {
        for (int c=0;c<grid->cols;c++) {
            ui_grid_cell_t *cell = &GRID_CELL(grid, r, c);
            if (!cell->item || (cell->halign!=-999 && cell->valign!=-999)) continue;

            if (cell->halign==-999) {
                cell->item->width = 0;
                for (int cs=0;cs<cell->colspan;cs++) {
                    cell->item->width += grid->colwidths[c+cs];
                    if (cs > 0) cell->item->width += grid->colspacing[c+cs-1];
                }
            }
            if (cell->valign==-999) {
                cell->item->height = 0;
                for (int rs=0;rs<cell->rowspan;rs++) {
                    cell->item->height += grid->rowheights[r+rs];
                    if (rs > 0) cell->item->height += grid->rowspacing[r+rs-1];
                }
            }
        }
    }

    *width = pwidth;
    *height = pheight;

    return 1;
}


int ui_grid_colwidth(ui_grid_t *grid, int col) {
    if (col < 0 || col >= grid->cols) return -1;
    return grid->colwidths[col];
}

int ui_row_rowheight(ui_grid_t *grid, int row) {
    if (row < 0 || row >= grid->rows) return -1;
    return grid->rowheights[row];
}

void ui_grid_draw(ui_grid_t *grid, int offset_x, int offset_y, mat4f_t *proj) {
    int cx = offset_x + grid->element.x;
    int cy = offset_y + grid->element.y;

    for (int r=0;r<grid->rows;r++) {
        cx = offset_x + grid->element.x;
        for (int c=0;c<grid->cols;c++) {
            ui_grid_cell_t *cell = &GRID_CELL(grid, r, c);

            if (cell->item && cell->item->draw) {
                int item_width = 0;
                int item_height = 0;
                if (cell->item->get_preferred_size) {
                    cell->item->get_preferred_size(cell->item, &item_width, &item_height);
                }

                int extra_x = 0;
                int extra_y = 0;
                if (cell->halign==0) {
                    //extra_x += (int)(((float)grid->colwidths[c] / 2.f) - ((float)cell->item->width / 2.f));
                    int totalwidth = 0;
                    for (int cs=0;cs<cell->colspan;cs++) {
                        totalwidth += grid->colwidths[c + cs];
                        if (cs>0) totalwidth += grid->colspacing[c + cs - 1];
                    }
                    extra_x += (int)(((float)totalwidth / 2.f) - ((float)cell->item->width / 2.f));
                } else if (cell->halign==1) {
                    extra_x += grid->colwidths[c] - cell->item->width;
                } else if (cell->halign==999) {
                    int totalwidth = 0;
                    for (int cs=0;cs<cell->colspan;cs++) {
                        totalwidth += grid->colwidths[c + cs];
                        if (cs>0) totalwidth += grid->colspacing[c + cs - 1];
                    }
                    item_width = totalwidth;
                }

                if (cell->valign==0) {
                    int totalheight = 0;
                    for (int rs=0;rs<cell->rowspan;rs++) {
                        totalheight += grid->rowheights[r + rs];
                        if (rs>0) totalheight += grid->rowspacing[r + rs - 1];
                    }

                    extra_y += (int)(((float) totalheight / 2.f) - ((float)cell->item->height / 2.f));
                } else if (cell->valign==1) {
                    extra_y += grid->rowheights[r] - cell->item->height;
                } else if (cell->valign==999) {
                    int totalheight = 0;
                    for (int rs=0;rs<cell->rowspan;rs++) {
                        totalheight += grid->rowheights[r + rs];
                        if (rs>0) totalheight += grid->rowspacing[r + rs - 1];
                    }
                    item_height = totalheight;
                }

                if (item_height) cell->item->height = item_height;
                if (item_width ) cell->item->width  = item_width;

                ui_element_draw(cell->item, cx + extra_x, cy + extra_y, proj);                
            }
            cx += grid->colwidths[c] + grid->colspacing[c];
        }
        cy += grid->rowheights[r] + grid->rowspacing[r];
    }
}

/*** RST
Grid Layouts
============

.. lua:currentmodule:: eg-overlay-ui

Functions
---------
*/

#define lua_checkuigrid(L,ind) *(ui_grid_t**)luaL_checkudata(L, ind, "UIGridMetaTable")
void lua_pushuigrid(lua_State *L, ui_grid_t *grid);

int ui_grid_lua_new(lua_State *L);
int ui_grid_lua_del(lua_State *L);
int ui_grid_lua_attach(lua_State *L);
int ui_grid_lua_rowspacing(lua_State *L);
int ui_grid_lua_colspacing(lua_State *L);

void ui_grid_lua_register_ui_funcs(lua_State *L) {
    lua_pushcfunction(L, &ui_grid_lua_new);
    lua_setfield(L, -2, "grid");   
}

luaL_Reg ui_grid_lua_funcs[] = {
    "__gc"              , &ui_grid_lua_del,
    "attach"            , &ui_grid_lua_attach,
    "rowspacing"        , &ui_grid_lua_rowspacing,
    "colspacing"        , &ui_grid_lua_colspacing,
    "background"        , &ui_element_lua_background,
    "addeventhandler"   , &ui_element_lua_addeventhandler,
    "removeeventhandler", &ui_element_lua_removeeventhandler,
    NULL                ,  NULL
};

void lua_pushuigrid(lua_State *L, ui_grid_t *grid) {
    ui_grid_t **pgrid = lua_newuserdata(L, sizeof(ui_grid_t*));

    *pgrid = grid;

    if (luaL_newmetatable(L, "UIGridMetaTable")) {
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");

        lua_pushboolean(L, 1);
        lua_setfield(L, -2, "__is_uielement");

        luaL_setfuncs(L, ui_grid_lua_funcs, 0);
    }
    lua_setmetatable(L, -2);

    ui_element_ref(grid);
}

/*** RST
.. lua:function:: grid(rows, columns)

    Create a new :lua:class:`uigrid`.

    :rtype: uigrid

    .. versionhistory::
        :0.1.0: Added
*/
int ui_grid_lua_new(lua_State *L) {
    int rows = (int)luaL_checkinteger(L, 1);
    int cols = (int)luaL_checkinteger(L, 2);

    if (rows < 1) {
        return luaL_error(L, "eg-overlay-ui.grid: rows must be 1 or greater.");
    }

    if (cols < 1) {
        return luaL_error(L, "eg-overlay-ui.grid: columns must be 1 or greater.");
    }

    ui_grid_t *grid = ui_grid_new(rows, cols);

    lua_pushuigrid(L, grid);
    ui_element_unref(grid);

    return 1;
}

/*** RST
Classes
-------

.. lua:class:: uigrid

    A grid layout container. Grid layout containers arrange their child elements
    into an aligned grid or table.

    Grids must have a known number of columns and rows when they are created.
    This can not be changed after a grid is created.

    .. code-block:: lua
        :caption: Example

        local ui = require 'overlay-ui'
        local uih = require 'ui-helpers'

        -- a 3x3 grid
        local grid = ui.grid(3, 3)
        
        local a = uih.text('Top Left')
        local b = uih.text('Top Middle & Right')
        local c = uih.text('Middle')
        local d = uih.text_button('Bottom Left & Middle')
        local e = uih.text_button('Bottom Right')

        -- arrange elements in this pattern
        --
        -- +-------------+-----------------------+
        -- | Top Left    |    Top Middle & Right |
        -- +-------------+-----------------------+
        -- |       Middle         |              |
        -- +----------------------+ Bottom Right +
        -- | Bottom Left & Middle |              |
        -- +----------------------+--------------+

        grid:attach(a, 1, 1)                           -- Top Left
        grid:attach(b, 1, 2, 1, 2, 'end'   , 'start')  -- Top Middle & Right
        grid:attach(c, 2, 1, 1, 2, 'middle', 'start')  -- Middle
        grid:attach(d, 3, 1, 1, 2, 'middle', 'middle') -- Bottom Left & Middle
        grid:attach(e, 2, 3, 2, 1, 'middle', 'middle') -- Bottom Right
*/

int ui_grid_lua_del(lua_State *L) {
    ui_grid_t *grid = lua_checkuigrid(L, 1);

    ui_element_unref(grid);

    return 0;
}

/*** RST
    .. lua:method:: attach(uielement, row, column[, rowspan, colspan[, horizalign, vertalign]])

        Attach a UI element to this grid at the given ``row`` and ``column``. If
        there was already an element at this location it will be removed first.

        By default, the element will only occupy one cell. This can be changed
        by providing a value for ``rowspan`` and ``colspan``. While both are
        optional, both must be provided if either is to be set.

        The element will be aligned to the top left of the cell by default. This
        can also be changed by proving ``horizalign`` and ``vertalign`` . Like
        the col/row span arguments, these are both optional but both must be
        provided if either is to be set.

        :param uielement: A UI element.
        :param integer row: Row number. This must be between 1 and the number of
            rows specified in :lua:func:`grid`.
        :param integer column: Column number. This must be between 1 and the
            number of columns specified in :lua:func:`grid`.
        :param integer rowspan: (Optional) The number of rows this element will
            span. This must be between 1 and the number of remaining rows in the
            grid. Default: ``1``
        :param integer colspan: (Optional) The number of columns this element
            will span. This argument must be present when ``rowspan`` is. This
            must be between 1 and the number of remaining columns remaining in
            the grid. Default: ``1``
        :param string horizalign: Horizontal alignment. ``'start'``, ``'middle'``, ``'end'``, or ``'fill'``.
        :param string vertalign: Vertical alignment. ``'start'``, ``'middle'``, ``'end'``, or ``'fill'``.

        .. versionhistory::
            :0.1.0: Added
*/
int ui_grid_lua_attach(lua_State *L) {
    int args = lua_gettop(L);

    if (args!=4 && args!=6 && args!=8) {
        // -1 because we aren't counting 'self'
        return luaL_error(L,"uigrid:attach() takes 3, 5 or 7 arguments, got %d", args - 1);
    }

    ui_grid_t *grid = lua_checkuigrid(L, 1);

    ui_element_t *element = lua_checkuielement(L, 2);

    int row = (int)luaL_checkinteger(L, 3);
    int col = (int)luaL_checkinteger(L, 4);

    int rowspan = 1;
    int colspan = 1;
    int horizalign = -1;
    int vertalign = -1;

    if (args==6 || args==8) {
        rowspan = (int)luaL_checkinteger(L, 5);
        colspan = (int)luaL_checkinteger(L, 6);
    }

    if (args==8) {
        horizalign = lua_checkuialign(L, 7);
        vertalign = lua_checkuialign(L, 8);
    }

    if (row < 1 || row > grid->rows) return luaL_error(L, "row out of range. %d [1, %d]", row, grid->rows);
    if (col < 1 || col > grid->cols) return luaL_error(L, "column out of range.");

    // Lua index starts at 1, we start at 0
    row--;
    col--;

    ui_grid_attach(grid, element, row, col, rowspan, colspan, horizalign, vertalign);

    return 0;
}


/*** RST
    .. lua:method:: rowspacing([row,] spacing)

        Set the spacing between ``row`` and the next row to ``spacing`` pixels.

        If ``row`` is omitted, all rows will be set to use ``spacing``

        :param integer row: (Optional)
        :param integer spacing:

        .. versionhistory::
            :0.1.0: Added
*/
int ui_grid_lua_rowspacing(lua_State *L) {
    ui_grid_t *grid = lua_checkuigrid(L, 1);
    int row = (int)luaL_checkinteger(L, 2);
    int spacing = 0;

    if (lua_gettop(L)==2) {
        // no row, first arg is spacing
        if (row < 0) return luaL_error(L, "spacing must be 0 or greater.");

        // set all but the last row
        for (int r=0;r<grid->rows-1;r++) {
            ui_grid_rowspacing(grid, r, row);
        }

        return 0;
    }

    spacing = (int)luaL_checkinteger(L, 3);

    if (row < 1 || row > grid->rows) return luaL_error(L, "row out of range");

    if (spacing < 0) return luaL_error(L, "spacing must be 0 or greater.");

    row--;

    ui_grid_rowspacing(grid, row, spacing);

    return 0;
}

/*** RST
    .. lua:method:: colspacing([column,] spacing)

        Set the spacing between ``column`` and the next column to ``spacing``
        pixels.

        If ``column`` is omitted, all columns will use ``spacing``

        :param integer column:
        :param integer spacing:

        .. versionhistory::
            :0.1.0: Added
*/
int ui_grid_lua_colspacing(lua_State *L) {
    ui_grid_t *grid = lua_checkuigrid(L, 1);
    int col = (int)luaL_checkinteger(L, 2);
    int spacing = 0;

    if (lua_gettop(L)==2) {
        // no col, first arg is spacing
        if (col < 0) return luaL_error(L, "spacing must be 0 or greater.");

        // set all but the last column
        for (int c=0;c<grid->cols-1;c++) {
            ui_grid_colspacing(grid, c, col);
        }

        return 0;
    }
    
    spacing = (int)luaL_checkinteger(L, 3);

    if (col < 1 || col > grid->cols) return luaL_error(L, "column out of range");

    if (spacing < 0) return luaL_error(L, "spacing must be 0 or greater.");

    col--;

    ui_grid_colspacing(grid, col, spacing);

    return 0;
}

/*** RST

    .. include:: /docs/_include/ui_element_color.rst

    .. include:: /docs/_include/ui_element_eventhandlers.rst
*/
