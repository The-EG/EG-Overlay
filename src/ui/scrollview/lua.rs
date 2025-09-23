// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT

/*** RST
Scroll Elements
===============

.. lua:currentmodule:: ui

Scrollview elements allow large elements to be displayed in a smaller area by
providing scroll bars. A scrollview only has a single child element, layout
elements like :doc:`boxes <../uibox/lua>` or :doc:`grids <../grid/lua>` should
be used to create complex content.

A new scrollview can be created with the :lua:func:`scrollview` function in the
:lua:mod:`ui` module.

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
use crate::ui::scrollview;

use crate::overlay::lua::luawarn;

const SCROLLVIEW_METATABLE_NAME: &str = "ui::ScrollView";

const UI_MOD_FUNCS: &[luaL_Reg] = luaL_Reg_list!{
    c"scrollview", new_scrollview,
};

const SCROLLVIEW_FUNCS: &[luaL_Reg] = luaL_Reg_list!{
    c"child"  , child,
    c"scrolly", scroll_y,
};

pub fn register_module_functions(l: &lua_State) {
    lua::L::setfuncs(l, UI_MOD_FUNCS, 0);
}

/*** RST
.. lua:function:: scrollview()

    Create a new :lua:class:`uiscrollview`.

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn new_scrollview(l: &lua_State) -> i32 {
    let sv = scrollview::ScrollView::new();

    ui::lua::pushelement(l, &sv, SCROLLVIEW_METATABLE_NAME, Some(SCROLLVIEW_FUNCS));

    return 1;
}

/*** RST
Classes
-------

.. lua:class:: uiscrollview

    A scrollview allows a large UI element to be displayed in a small area
    using scrollbars.

    .. lua:method:: child(element)

        Set the child element of this scrollview.

        :param uielement child:

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn child(l: &lua_State) -> i32 {
    let sve = unsafe { ui::lua::checkelement(l, 1) };
    let sv = unsafe { checkscrollview(l, &sve) };

    let c = unsafe { ui::lua::checkelement(l, 2) };

    sv.inner.lock().unwrap().child = Some((*c).clone());

    return 0;
}

/*** RST
    .. lua:method:: scrolly([value])

        Get or set the Y scroll position.

        :param number value: (Optional) The Y position to scroll to, as a value
            between 0.0 (the top) and 1.0 (the bottom).
        :returns: A number if ``value`` is not specified or ``nil``.

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn scroll_y(l: &lua_State) -> i32 {
    let sve = unsafe { ui::lua::checkelement(l, 1) };
    let sv = unsafe { checkscrollview(l, &sve) };

    if lua::gettop(l) >= 2 {
        let mut y = unsafe { lua::L::checknumber(l, 2) };

        if y < 0.0 {
            luawarn!(l, "scrolly value must be between 0 and 1.");
            y = 0.0;
        }

        if y > 1.0 {
            luawarn!(l, "scrolly value must be bewtween 0 and 1.");
            y = 1.0;
        }

        let mut inner = sv.inner.lock().unwrap();

        if let Some(child) = inner.child.as_ref() {
            inner.child_height = child.get_preferred_height();
        }

        let max_y = inner.child_height - inner.height + 10;

        inner.disp_y = (max_y as f64 * y) as i64;

        return 0;
    } else {
        let inner = sv.inner.lock().unwrap();

        if inner.child_height <= inner.height - 10 {
            lua::pushnumber(l, 1.0);
        } else {
            let max_y = inner.child_height - inner.height + 10;

            let cury = inner.disp_y as f64 / max_y as f64;

            lua::pushnumber(l, cury);
        }

        return 1;
    }
}

/*** RST
    .. note::

        The following methods are inherited from :lua:class:`uielement`
    
    .. include:: /docs/_include/uielement.rst
*/

unsafe fn checkscrollview<'a>(l: &lua_State, element: &'a ManuallyDrop<Arc<ui::Element>>) -> &'a scrollview::ScrollView {
    if let Some(s) = element.as_scrollview() { s }
    else {
        lua::pushstring(l, "element is not a scrollview.");
        unsafe { lua::error(l); }
        panic!("Element is not a scrollview.");
    }
}

