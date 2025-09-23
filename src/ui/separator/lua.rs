// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT

/*** RST
Separator Elements
==================

.. lua:currentmodule:: ui

Separators provide a visual break between elements by rendering a simple line
either horizontally or vertically.

.. important::
    
    Separators do not request a large size, so they should be used with ``fill``
    alignment when used with :doc:`boxes <../uibox/lua>` and :doc:`grids <../grid/lua>`.

A new separator can be created with the :lua:func:`separator` function in the
:lua:mod:`ui` module.

Functions
---------
*/
use std::sync::Arc;
use std::mem::ManuallyDrop;

use crate::lua;
use crate::lua::lua_State;
use crate::lua::luaL_Reg;
use crate::lua::luaL_Reg_list;

use crate::ui;
use crate::ui::separator::Separator;

const SEP_METATABLE_NAME: &str = "ui::Separator";

const UI_MOD_FUNCS: &[luaL_Reg] = luaL_Reg_list!{
    c"separator", new_separator
};

const SEP_FUNCS: &[luaL_Reg] = luaL_Reg_list!{
    c"thickness", thickness,
};

pub fn register_module_functions(l: &lua_State) {
    lua::L::setfuncs(l, UI_MOD_FUNCS, 0);
}

/*** RST
.. lua:function:: separator(orientation)

    Create a new :lua:class:`uiseparator`.

    :param string orientation: ``'horizontal'`` or ``'vertical'``.
    :rtype: uiseparator

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn new_separator(l: &lua_State) -> i32 {
    let orientation = unsafe { ui::lua::checkorientation(l, 1) };

    let sep = Separator::new(orientation);

    ui::lua::pushelement(l, &sep, SEP_METATABLE_NAME, Some(SEP_FUNCS));

    return 1;
}

/*** RST
Classes
-------

.. lua:class:: uiseparator

    .. lua:method:: thickness(pixels)

        Set the thickness of this separator.

        :param integer pixels:

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn thickness(l: &lua_State) -> i32 {
    let sep_e = unsafe { ui::lua::checkelement(l, 1) };
    let sep = unsafe { checkseparator(l, &sep_e) };

    let thickness = unsafe { lua::L::checkinteger(l, 2) };

    sep.inner.lock().unwrap().thickness = thickness;

    return 0;
}

/*** RST
    .. note::

        The following methods are inherited from :lua:class:`uielement`
    
    .. include:: /docs/_include/uielement.rst
*/

unsafe fn checkseparator<'a>(l: &lua_State, element: &'a ManuallyDrop<Arc<ui::Element>>) -> &'a Separator {
    if let Some(s) = element.as_separator() { s }
    else {
        lua::pushstring(l, "element is not a separator.");
        unsafe { _ = lua::error(l); }
        panic!("element is not a separator.");
    }
}

