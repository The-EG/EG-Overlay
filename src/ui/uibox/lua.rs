// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT

/*** RST
Box Layouts
===========

.. lua:currentmodule:: eg-overlay-ui

Box layouts display multiple child elements in either a row (``'horizontal'``)
or a column (``'vertical'``).

Boxes will automatically resize to contain all child elements if possible.

A new box can be created with the :lua:func:`box` function in the
:lua:mod:`eg-overlay-ui` module.

Child Alignment
---------------

Each child added to a box must have an alignment value set. This alignment
affects how the child is positioned within the box relative to the box and other
children.

.. important::

    Alignment is perpendicular to the orientation of the box. A vertical box will
    use alignment to horizontally position children.

The following values are valid alignments:

+--------+----------------------------------------------------------------------------+
| Value  | Description                                                                |
+========+============================================================================+
| start  | Top or Left                                                                |
+--------+----------------------------------------------------------------------------+
| middle | Center                                                                     |
+--------+----------------------------------------------------------------------------+
| end    | Bottom or Right                                                            |
+--------+----------------------------------------------------------------------------+
| fill   | Fill the available area. Some elements will be drawn the same as ``start`` |
+--------+----------------------------------------------------------------------------+

Alignment in a vertical box:

.. code-block:: text

    +------------------------------+
    | [start]                      |
    |           [middle]           |
    |                        [end] |
    | [           fill           ] |
    +------------------------------+

Alignment in a horizontal box:

.. code-block:: text

    +-------------------------------+
    | [start]                [      |
    |         [middle]        fill  |
    |                  [end]      ] |
    +-------------------------------+


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
use crate::ui::uibox::Box;

const BOX_METATABLE_NAME: &str = "ui::Box";

const UI_MOD_FUNCS: &[luaL_Reg] = luaL_Reg_list!{
    c"box", new_box
};

const BOX_FUNCS: &[luaL_Reg] = luaL_Reg_list!{
    c"pushfront"    , push_front,
    c"pushback"     , push_back,

    c"insertbefore" , insert_before,
    c"insertafter"  , insert_after,

    c"popfront"     , pop_front,
    c"popback"      , pop_back,

    c"removeitem"   , remove_item,

    c"__len"        , len,
    c"len"          , len,

    c"paddingleft"  , padding_left,
    c"paddingright" , padding_right,
    c"paddingtop"   , padding_top,
    c"paddingbottom", padding_bottom,

    c"spacing"      , spacing,
    c"alignment"    , alignment,
};

pub fn register_module_functions(l: &lua_State) {
    lua::L::setfuncs(l, UI_MOD_FUNCS, 0);
}

/*** RST
.. lua:function:: box(orientation)
    
    Create a new box layout container.

    Box layout containers arrange their child elements sequentially in either a
    vertical or horizontal fashion.

    :param string orientation: The layout orientation. Either ``'vertical'`` or ``'horizontal'``.
    :return: A new :lua:class:`uibox`.

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn new_box(l: &lua_State) -> i32 {
    let orientation = unsafe { ui::lua::checkorientation(l, 1) };

    let b = Box::new(orientation);

    ui::lua::pushelement(l, &b, BOX_METATABLE_NAME, Some(BOX_FUNCS));

    return 1;
}

/*** RST
Classes
-------

.. lua:class:: uibox

    A box layout container.

    Box layout containers arrange their child elements sequentially in either a
    vertical or horizontal fashion.

    .. lua:method:: pushfront(element, alignment, expand)

        Add an element to the top/left of this box.

        :param uielement element: Child element
        :param string alignment: One of: ``'start'``, ``'middle'``, ``'end'``, or ``'fill'``
        :param boolean expand: Sets if the element should use extra available space

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn push_front(l: &lua_State) -> i32 {
    let box_e = unsafe { ui::lua::checkelement(l, 1) };
    let bx = unsafe { checkbox(l, &box_e) };

    let item = unsafe { ui::lua::checkelement(l, 2) };
    let align = unsafe { lua::L::checkstring(l, 3) };
    let expand = lua::toboolean(l, 4);

    bx.push_front(&item, ui::ElementAlignment::from(align.as_str()), expand);

    return 0;
}

/*** RST
    .. lua:method:: pushback(element, alignment, expand)

        Add an element to the bottom/right of this box.

        :param uielement element: Child element
        :param string alignment: One of: ``'start'``, ``'middle'``, ``'end'``, or ``'fill'``
        :param boolean expand: Sets if the element should use extra available space

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn push_back(l: &lua_State) -> i32 {
    let box_e = unsafe { ui::lua::checkelement(l, 1) };
    let bx = unsafe { checkbox(l, &box_e) };

    let item = unsafe { ui::lua::checkelement(l, 2) };
    let align = unsafe { lua::L::checkstring(l, 3) };
    let expand = lua::toboolean(l, 4);

    bx.push_back(&item, ui::ElementAlignment::from(align.as_str()), expand);

    return 0; 
}

/*** RST
    .. lua:method:: insertbefore(before, element, alignment, expand)

        Insert ``element`` before ``before``.

        If ``before`` is not in this box, ``element`` will not be added and this
        method will return ``false``. ``true`` is returned on success.

        :param uielement before:
        :param uielement element:
        :param string alignment: One of: ``'start'``, ``'middle'``, ``'end'``, or ``'fill'``
        :param boolean expand: Sets if the element should use extra available space

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn insert_before(l: &lua_State) -> i32 {
    let box_e = unsafe { ui::lua::checkelement(l, 1) };
    let bx = unsafe { checkbox(l, &box_e) };

    let before = unsafe { ui::lua::checkelement(l, 2) };
    let item = unsafe { ui::lua::checkelement(l, 3) };
    let align = unsafe { lua::L::checkstring(l, 4) };
    let expand = lua::toboolean(l, 5);

    let r = bx.insert_before(&before, &item, ui::ElementAlignment::from(align.as_str()), expand);

    lua::pushboolean(l, r);

    return 1;
}

/*** RST
    .. lua:method:: insertafter(after, element, alignment, expand)

        Insert ``element`` after ``after``.

        If ``after`` is not in this box, ``element`` will not be added and this
        method will return ``false``. ``true`` is returned on success.

        :param uielement after:
        :param uielement element:
        :param string alignment: One of: ``'start'``, ``'middle'``, ``'end'``, or ``'fill'``
        :param boolean expand: Sets if the element should use extra available space

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn insert_after(l: &lua_State) -> i32 {
    let box_e = unsafe { ui::lua::checkelement(l, 1) };
    let bx = unsafe { checkbox(l, &box_e) };

    let after = unsafe { ui::lua::checkelement(l, 2) };
    let item = unsafe { ui::lua::checkelement(l, 3) };
    let align = unsafe { lua::L::checkstring(l, 4) };
    let expand = lua::toboolean(l, 5);

    let r = bx.insert_after(&after, &item, ui::ElementAlignment::from(align.as_str()), expand);

    lua::pushboolean(l, r);

    return 1;
}

/*** RST
    .. lua:method:: popfront()

        Remove the first element of this box.

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn pop_front(l: &lua_State) -> i32 {
    let box_e = unsafe { ui::lua::checkelement(l, 1) };
    let bx = unsafe { checkbox(l, &box_e) };

    bx.pop_front();

    return 0;
}

/*** RST
    .. lua:method:: popback()

        Remove the last element of this box.

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn pop_back(l: &lua_State) -> i32 {
    let box_e = unsafe { ui::lua::checkelement(l, 1) };
    let bx = unsafe { checkbox(l, &box_e) };

    bx.pop_back();

    return 0;
}

/*** RST
    .. lua:method:: removeitem(element)

        Remove the given element from this box.

        :param uielement element:
        
        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn remove_item(l: &lua_State) -> i32 {
    let box_e = unsafe { ui::lua::checkelement(l, 1) };
    let bx = unsafe { checkbox(l, &box_e) };
    let ele = unsafe { ui::lua::checkelement(l, 2) };

    bx.remove_item(&ele);

    return 0;
}

/*** RST
    .. lua:method:: len()

        Returns the number of items in this box.

        .. note::
            
            This is also called when using the Lua length operator on the box (``#``).
        
        :rtype: integer

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn len(l: &lua_State) -> i32 {
    let box_e = unsafe { ui::lua::checkelement(l, 1) };
    let bx = unsafe { checkbox(l, &box_e) };

    lua::pushinteger(l, bx.inner.lock().unwrap().items.len() as i64);

    return 1;
}

/*** RST
    .. lua:function:: paddingleft(padding)

        Set the amount of space between the left side of the box and the first
        element, in pixels.

        :param integer padding:

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn padding_left(l: &lua_State) -> i32 {
    let box_e = unsafe { ui::lua::checkelement(l, 1) };
    let bx = unsafe { checkbox(l, &box_e) };

    let p = unsafe { lua::L::checkinteger(l, 2) };

    bx.inner.lock().unwrap().padding_left = p;

    return 0;
}

/*** RST
    .. lua:function:: paddingright(padding)

        Set the amount of space between the right side of the box and the last
        element, in pixels.

        :param integer padding:

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn padding_right(l: &lua_State) -> i32 {
    let box_e = unsafe { ui::lua::checkelement(l, 1) };
    let bx = unsafe { checkbox(l, &box_e) };

    let p = unsafe { lua::L::checkinteger(l, 2) };

    bx.inner.lock().unwrap().padding_right = p;

    return 0;
}

/*** RST
    .. lua:function:: paddingtop(padding)

        Set the amount of space between the top side of the box and the first
        element, in pixels.

        :param integer padding:

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn padding_top(l: &lua_State) -> i32 {
    let box_e = unsafe { ui::lua::checkelement(l, 1) };
    let bx = unsafe { checkbox(l, &box_e) };

    let p = unsafe { lua::L::checkinteger(l, 2) };

    bx.inner.lock().unwrap().padding_top = p;

    return 0;
}

/*** RST
    .. lua:function:: paddingbottom(padding)

        Set the amount of space between the bottom side of the box and the last
        element, in pixels.

        :param integer padding:

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn padding_bottom(l: &lua_State) -> i32 {
    let box_e = unsafe { ui::lua::checkelement(l, 1) };
    let bx = unsafe { checkbox(l, &box_e) };

    let p = unsafe { lua::L::checkinteger(l, 2) };

    bx.inner.lock().unwrap().padding_bottom = p;

    return 0;
}

/*** RST
    .. lua:function:: spacing(amount)

        Set the amount of space between elements in this box, in pixels.

        :param integer amount:

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn spacing(l: &lua_State) -> i32 {
    let box_e = unsafe { ui::lua::checkelement(l, 1) };
    let bx = unsafe { checkbox(l, &box_e) };

    let s = unsafe { lua::L::checkinteger(l, 2) };

    bx.inner.lock().unwrap().spacing = s;

    return 0;
}

/*** RST
    .. lua:method:: alignment(align)

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn alignment(l: &lua_State) -> i32 {
    let box_e = unsafe { ui::lua::checkelement(l, 1) };
    let bx = unsafe { checkbox(l, &box_e) };

    let align = unsafe { lua::L::checkstring(l, 2) };

    bx.inner.lock().unwrap().alignment = ui::ElementAlignment::from(align.as_str());

    return 0;
}

/*** RST
    .. note::

        The following methods are inherited from :lua:class:`uielement`

    .. include:: /docs/_include/uielement.rst
*/

unsafe fn checkbox<'a>(l: &lua_State, element: &'a ManuallyDrop<Arc<ui::Element>>) -> &'a Box {
    if let Some(b) = element.as_box() { b }
    else {
        lua::pushstring(l, "element is not a box.");
        unsafe { _ = lua::error(l); }
        panic!("element is not a box.");
    }
}

