// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT

/*** RST
Grid Layouts
============

.. lua:currentmodule:: eg-overlay-ui

Grid layouts display multiple child elements in a table or grid.

Grids will automatically resize to contain all child elements if possible and
each row and column will automatically resize to display the children as well.

A new grid can be created with the :lua:func:`grid` function in the
:lua:mod:`eg-overlay-ui` module.

.. seealso::

    See the example for :lua:class:`uigrid` for details on child alignment
    and row/column spanning.

Functions
---------
*/
#[allow(unused_imports)]
use crate::logging::{debug, info, warn, error};

use std::sync::Arc;
use std::mem::ManuallyDrop;

use crate::lua;
use crate::lua::lua_State;
use crate::lua::luaL_Reg;
use crate::lua::luaL_Reg_list;

use crate::ui;
use crate::ui::grid::Grid;

const GRID_METATABLE_NAME: &str = "ui::Grid";

const UI_MOD_FUNCS: &[luaL_Reg] = luaL_Reg_list!{
    c"grid", new_grid,
};

const GRID_FUNCS: &[luaL_Reg] = luaL_Reg_list!{
    c"attach"    , attach,
    c"detach"    , detach,
    c"rowspacing", rowspacing,
    c"colspacing", colspacing,
};

pub fn register_module_functions(l: &lua_State) {
    lua::L::setfuncs(l, UI_MOD_FUNCS, 0);
}

/*** RST
.. lua:function:: grid(rows, cols)

    Create a new :lua:class:`uigrid`

    :rtype: uigrid

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn new_grid(l: &lua_State) -> i32 {
    let rows = unsafe { lua::L::checkinteger(l, 1) };
    let cols = unsafe { lua::L::checkinteger(l, 2) };

    if rows < 1 {
        lua::pushstring(l, "rows must be greater than 0.");
        unsafe { return lua::error(l); }
    }

    if cols < 1 {
        lua::pushstring(l, "cols must be greater than 0.");
        unsafe { return lua::error(l); }
    }

    let g = Grid::new(rows, cols);

    ui::lua::pushelement(l, &g, GRID_METATABLE_NAME, Some(GRID_FUNCS));

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

        local ui = require 'eg-overlay-ui'

        local function txt(t)
            return ui.text(t, 0xFFFFFFFF, ui.fonts.regular)
        end

        -- a 3x3 grid
        local grid = ui.grid(3, 3)
        
        local a = txt('Top Left', )
        local b = txt('Top Middle & Right')
        local c = txt('Middle')
        local d = txt('Bottom Left & Middle')
        local e = txt('Bottom Right')

        -- arrange elements in this pattern
        --
        -- +-------------+-----------------------+
        -- | Top Left    |    Top Middle & Right |
        -- +-------------+-----------------------+
        -- |       Middle         |              |
        -- +----------------------+ Bottom Right +
        -- | Bottom Left & Middle |              |
        -- +----------------------+--------------+

        grid:attach(a, 1, 1, 1, 1, 'start' , 'start')  -- Top Left
        grid:attach(b, 1, 2, 1, 2, 'end'   , 'start')  -- Top Middle & Right
        grid:attach(c, 2, 1, 1, 2, 'middle', 'start')  -- Middle
        grid:attach(d, 3, 1, 1, 2, 'middle', 'middle') -- Bottom Left & Middle
        grid:attach(e, 2, 3, 2, 1, 'middle', 'middle') -- Bottom Right

    .. lua:method:: attach(uielement, row, col, rowspan, colspan, halign, valign)

        Attach a UI element to this grid at the given ``row`` and ``col``. If
        there was already an element at this location it will be removed first.

        Normally the element will only occupy one cell. This can be changed
        by providing appropriate values for ``rowspan`` and ``colspan``.

        Element alignment is specified by ``halign`` and ``valign`` .

        :param uielement: A UI element.
        :param integer row: Row number. This must be between 1 and the number of
            rows specified in :lua:func:`grid`.
        :param integer column: Column number. This must be between 1 and the
            number of columns specified in :lua:func:`grid`.
        :param integer rowspan: The number of rows this element will span. This
            must be between 1 and the number of remaining rows in the grid.
        :param integer colspan: The number of columns this element will span.
            This must be between 1 and the number of remaining columns remaining
            in the grid.
        :param string horizalign: Horizontal alignment. ``'start'``, ``'middle'``,
            ``'end'``, or ``'fill'``.
        :param string vertalign: Vertical alignment. ``'start'``, ``'middle'``,
            ``'end'``, or ``'fill'``.

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn attach(l: &lua_State) -> i32 {
    let grid_e = unsafe { ui::lua::checkelement(l, 1) };
    let grid = unsafe { checkgrid(l, &grid_e) };

    let e = unsafe { ui::lua::checkelement(l, 2) };

    let row = unsafe { lua::L::checkinteger(l, 3) };
    let col = unsafe { lua::L::checkinteger(l, 4) };

    let rowspan = unsafe { lua::L::checkinteger(l, 5) };
    let colspan = unsafe { lua::L::checkinteger(l, 6) };

    let halign = unsafe { ui::lua::checkalign(l, 7) };
    let valign = unsafe { ui::lua::checkalign(l, 8) };

    let item = ui::grid::GridItem {
        element: (*e).clone(),
        rowspan: rowspan,
        colspan: colspan,
        halign : halign,
        valign : valign,
    };

    grid.inner.lock().unwrap().set_item(row - 1, col - 1, Some(item));

    return 0;
}

/*** RST
    .. lua:method:: detach(row, col)

        Remove the element located at ``row`` and ``col``.

        If no element was located in this position, this function will do nothing.

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn detach(l: &lua_State) -> i32 {
    let grid_e = unsafe { ui::lua::checkelement(l, 1) };
    let grid = unsafe { checkgrid(l, &grid_e) };

    let row = unsafe { lua::L::checkinteger(l, 3) };
    let col = unsafe { lua::L::checkinteger(l, 4) };

    grid.inner.lock().unwrap().set_item(row, col, None);

    return 0;
}

/*** RST
    .. lua:method:: rowspacing([row,] spacing)

        Set the spacing between ``row`` and the next row to ``spacing`` pixels.

        If ``row`` is omitted, all rows will be set to use ``spacing``.

        :param integer row: (Optional)
        :param integer spacing:

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn rowspacing(l: &lua_State) -> i32 {
    let grid_e = unsafe { ui::lua::checkelement(l, 1) };
    let grid = unsafe { checkgrid(l, &grid_e) };

    let row = unsafe { lua::L::checkinteger(l, 2) };

    if lua::gettop(l) == 2 {
        // no row, first arg is spacing, set all rows to this
        if row < 0 {
            lua::pushstring(l, "spacing must be 0 or greater.");
            return unsafe { lua::error(l) };
        }

        let mut g = grid.inner.lock().unwrap();

        for r in 0..((g.rows-1) as usize) {
            g.rowspacing[r] = row;
        }

        return 0;
    }

    let rows = grid.inner.lock().unwrap().rows;

    let spacing = unsafe { lua::L::checkinteger(l, 3) };

    if row < 1  || row >= rows {
        lua::pushstring(l, "row out of range.");
        return unsafe { lua::error(l) };
    }

    if spacing < 0 {
        lua::pushstring(l, "spacing must be 0 or greater.");
        return unsafe { lua::error(l) };
    }

    grid.inner.lock().unwrap().rowspacing[(row-1) as usize] = spacing;

    return 0;
}

/*** RST
    .. lua:method:: colspacing([column,] spacing)

        Set the spacing between ``column`` and the next column to ``spacing``
        pixels.

        If ``column`` is omitted, all columns will use ``spacing``.

        :param integer column:
        :param integer spacing:

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn colspacing(l: &lua_State) -> i32 {
    let grid_e = unsafe { ui::lua::checkelement(l, 1) };
    let grid = unsafe { checkgrid(l, &grid_e) };

    let col = unsafe { lua::L::checkinteger(l, 2) };

    if lua::gettop(l) == 2 {
        // no col, first arg is spacing, set all cols to this
        if col < 0 {
            lua::pushstring(l, "spacing must be 0 or greater.");
            return unsafe { lua::error(l) };
        }

        let mut g = grid.inner.lock().unwrap();

        for c in 0..((g.cols-1) as usize) {
            g.colspacing[c] = col;
        }

        return 0;
    }

    let cols = grid.inner.lock().unwrap().cols;

    let spacing = unsafe { lua::L::checkinteger(l, 3) };

    if col < 1 || col >= cols {
        lua::pushstring(l, "col out of range.");
        return unsafe { lua::error(l) };
    }

    if spacing < 0 {
        lua::pushstring(l, "spacing must be 0 or greater.");
        return unsafe { lua::error(l) };
    }

    grid.inner.lock().unwrap().colspacing[(col-1) as usize] = spacing;

    return 0;
}

/*** RST
    
    .. note::

        The following methods are inherited from :lua:class:`uielement`

    .. include:: /docs/_include/uielement.rst
*/

unsafe fn checkgrid<'a>(l: &lua_State, element: &'a ManuallyDrop<Arc<ui::Element>>) -> &'a Grid {
    if let Some(g) = element.as_grid() { g }
    else {
        lua::pushstring(l, "element is not a grid.");
        unsafe { _ = lua::error(l); }
        panic!("element is not a grid.");
    }
}

