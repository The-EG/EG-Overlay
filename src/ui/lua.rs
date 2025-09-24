// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT

//! UI Lua API

use std::sync::Arc;
use std::sync::Weak;

use std::mem::ManuallyDrop;

use crate::ui;

#[allow(unused_imports)]
use crate::logging::{debug, info, warn, error};

use crate::lua;
use crate::lua::lua_State;
use crate::lua::luaL_Reg;
use crate::lua::luaL_Reg_list;

#[doc(hidden)]
const UI_MOD_FUNCS: &[luaL_Reg] = luaL_Reg_list!{
    c"mouseposition"     , mouse_position,
    c"iconcodepoint"     , icon_codepoint,
    c"color"             , get_color,
    c"overlaysize"       , overlay_size,
};

#[doc(hidden)]
pub const ELEMENT_FUNCS: &[luaL_Reg] = luaL_Reg_list!{
    c"__gc"     , element_gc,
    c"x"        , element_x,
    c"y"        , element_y,
    c"width"    , element_width,
    c"height"   , element_height,
    c"bgcolor"  , element_bg_color,
};

/// Checks if the value at the index is a UI Element and returns it if so.
///
/// If the value is not an element a Lua error is raised. [checkelement] should
/// be used instead of this function.
pub unsafe fn checkelement_ptr(l: &lua_State, ind: i32) -> *mut *const ui::Element {
    if lua::luatype(l, ind) != lua::LuaType::LUA_TUSERDATA {
        lua::pushstring(l, format!("Argument {} is not a UI Element. (not userdata)", ind).as_str());
        unsafe { lua::error(l); }
    }

    if lua::getfield(l, ind, "__is_uielement")!=lua::LuaType::LUA_TBOOLEAN {
        lua::pushstring(l, format!("Argument {} is not a UI Element. (missing __is_uielement)", ind).as_str());
        unsafe { lua::error(l); }
    }

    if !lua::toboolean(l, -1) {
        lua::pop(l, 1);
        lua::pushstring(l, format!("Argument {} is not a UI Element. (__is_uielement != true)", ind).as_str());
        unsafe { lua::error(l); }
    } else {
        lua::pop(l, 1);
    }

    unsafe { std::mem::transmute(lua::touserdata(l, ind)) }
}

/// Checks if the value at the index is a UI Element and returns it if so.
///
/// If the value is not an element a Lua error is raised.
pub unsafe fn checkelement(l: &lua_State, ind: i32) -> ManuallyDrop<Arc<ui::Element>> {
    unsafe { ManuallyDrop::new(Arc::from_raw(*checkelement_ptr(l, ind))) }
}

pub fn pushelement(l: &lua_State, element: &Arc<ui::Element>, name: &str, funcs: Option<&[luaL_Reg]>) {
    let e_ptr = Arc::into_raw(element.clone()); // strong_count ++

    let lua_e_ptr: *mut *const ui::Element = unsafe {
        std::mem::transmute(lua::newuserdatauv(l, std::mem::size_of::<*const ui::Element>(), 0))
    };

    unsafe { *lua_e_ptr = e_ptr; }

    if lua::L::newmetatable(l, name) {
        lua::pushvalue(l, -1);
        lua::setfield(l, -2, "__index");

        lua::pushboolean(l, true);
        lua::setfield(l, -2, "__is_uielement");

        lua::L::setfuncs(l, ELEMENT_FUNCS, 0);

        if let Some(f) = funcs {
            lua::L::setfuncs(l, f, 0);
        }
    }
    lua::setmetatable(l, -2);
}

pub unsafe fn checkcolor(l: &lua_State, ind: i32) -> ui::Color {
    ui::Color::from(unsafe { lua::L::checkinteger(l, ind) })
}

pub unsafe fn checkalign(l: &lua_State, ind: i32) -> ui::ElementAlignment {
    let nm = unsafe { lua::L::checkstring(l, ind) };

    match nm.as_str() {
        "start"  => ui::ElementAlignment::Start,
        "middle" => ui::ElementAlignment::Middle,
        "end"    => ui::ElementAlignment::End,
        "fill"   => ui::ElementAlignment::Fill,
        _        => {
            lua::pushstring(l, format!("Unknown alignment: {}", nm).as_str());
            unsafe { let _ = lua::error(l); }
            panic!("unknown alignment");
        }
    }
}

pub unsafe fn checkorientation(l: &lua_State, ind: i32) -> ui::ElementOrientation {
    let nm = unsafe { lua::L::checkstring(l, ind) };

    match nm.as_str() {
        "horizontal" => ui::ElementOrientation::Horizontal,
        "vertical"   => ui::ElementOrientation::Vertical,
        _            => {
            lua::pushstring(l, format!("Unknown orientation: {}:", nm).as_str());
            unsafe { let _ = lua::error(l); }
            panic!("unknown orientation.");
        },
    }
}

#[doc(hidden)]
pub unsafe extern "C" fn open_module(l: &lua_State) -> i32 {
    let ui_ptr = Weak::into_raw(Arc::downgrade(&crate::overlay::ui()));

    lua::newtable(l);

    unsafe { lua::pushlightuserdata(l, ui_ptr as *const std::ffi::c_void); }
    lua::L::setfuncs(l, UI_MOD_FUNCS, 1);

    crate::ui::font::lua::register_module_functions(l);
    crate::ui::text::lua::register_module_functions(l);
    crate::ui::window::lua::register_module_functions(l);
    crate::ui::uibox::lua::register_module_functions(l);
    crate::ui::grid::lua::register_module_functions(l);
    crate::ui::separator::lua::register_module_functions(l);
    crate::ui::button::lua::register_module_functions(l);
    crate::ui::scrollview::lua::register_module_functions(l);
    crate::ui::entry::lua::register_module_functions(l);
    crate::ui::menu::lua::register_module_functions(l);

    return 1;
}

/*** RST
ui
==

.. lua:module:: ui

.. code-block:: lua

    local ui = require 'ui'

.. toctree::
    :caption: UI Elements
    :maxdepth: 1
    :hidden:

    font/lua
    window/lua
    uibox/lua
    grid/lua
    text/lua
    entry/lua
    scrollview/lua
    separator/lua
    button/lua
    menu/lua


The `ui` module is used to create UI elements for overlay modules.

Nearly all modules that display a UI will have at least one :lua:func:`window`.
:doc:`window/lua` use layout or scroll elements to display their child elements.

Top Level Elements:

* :doc:`window/lua`
* :doc:`menu/lua`

Layouts:

* :doc:`uibox/lua`
* :doc:`grid/lua`
* :doc:`scrollview/lua`

Child Elements:

* :doc:`text/lua`
* :doc:`entry/lua`
* :doc:`separator/lua`
* :doc:`button/lua`

*/

fn get_ui_upvalue(l: &lua_State) -> Arc<crate::ui::Ui> {
    let ui_ptr: *const crate::ui::Ui = unsafe {
        std::mem::transmute(lua::touserdata(l, lua::LUA_REGISTRYINDEX - 1))
    };

    let ui_weak = ManuallyDrop::new(unsafe { Weak::from_raw(ui_ptr) });

    ui_weak.upgrade().unwrap()
}


/*** RST

General UI Functions
--------------------

.. lua:function:: mouseposition()

    Return the last known mouse cursor position, relative to the overlay area.

    .. warning::

        This does not query the OS to get the actual position, instead it returns
        the position the cursor was in the last time an event came in.

        The overlay does not receive events if the target window (ie. Guild Wars 2)
        is not focused.

    :returns: 2 integers

    .. versionhistory::
        :0.3.0: Added
*/
#[doc(hidden)]
unsafe extern "C" fn mouse_position(l: &lua_State) -> i32 {
    let ui = get_ui_upvalue(l);

    lua::pushinteger(l, ui.last_mouse_x.load(std::sync::atomic::Ordering::Relaxed));
    lua::pushinteger(l, ui.last_mouse_y.load(std::sync::atomic::Ordering::Relaxed));

    return 2;
}

/*** RST
.. lua:function:: iconcodepoint(name)

    Return the utf-8 encoded codepoint for the given icon name.

    :rtype: string

    .. versionhistory::
        :0.3.0: Added
*/
#[doc(hidden)]
unsafe extern "C" fn icon_codepoint(l: &lua_State) -> i32 {
    let name = unsafe { lua::L::checkstring(l, 1) };
    let ui = get_ui_upvalue(l);

    match ui.icon_codepoint(&name) {
        Ok(cp) => lua::pushstring(l, &cp),
        Err(err) => {
            crate::overlay::lua::luawarn!(l, "Error while getting icon codepoint for {}: {}", name, err);
            lua::pushnil(l,);
        },
    }

    return 1;
}

/*** RST
.. lua:function:: color(name)

    Return the color for the given UI color name.

    :param string name:
    :rtype: integer

    ``name`` must be one of the following values:

    - ``windowBG``
    - ``windowBorder``
    - ``windowBorderHighlight``
    - ``text``
    - ``accentText``
    - ``entryBG``
    - ``entryHint``
    - ``buttonBG``
    - ``buttonBGHover``
    - ``buttonBGHighlight``
    - ``buttonBorder``
    - ``scrollThumb``
    - ``scrollThumbHighlight``
    - ``scrollBG``
    - ``menuBG``
    - ``menuBorder``
    - ``menuItemHover``
    - ``menuItemHighlight``

    .. versionhistory::
        :0.3.0: Added
*/
#[doc(hidden)]
unsafe extern "C" fn get_color(l: &lua_State) -> i32 {
    let name = unsafe { lua::L::checkstring(l, 1) };

    let o_settings = crate::overlay::settings();

    if let Some(c) = o_settings.get_u64(format!("overlay.ui.colors.{}", name).as_str()) {
        lua::pushinteger(l, c as i64);
    } else {
        crate::overlay::lua::luawarn!(l, "Unknown color: {}", name);
        lua::pushnil(l);
    }

    return 1;
}

/*** RST
.. lua:function:: overlaysize()

    Return the width and heigh of the overlay.

    :returns: 2 integers

    .. versionhistory::
        :0.3.0: Added
*/
#[doc(hidden)]
unsafe extern "C" fn overlay_size(l: &lua_State) -> i32 {
    let ui = get_ui_upvalue(l);

    let ui_size = ui.last_ui_size.lock().unwrap();

    lua::pushinteger(l, ui_size.0 as i64);
    lua::pushinteger(l, ui_size.1 as i64);

    return 2;
}

/*** RST
UI Elements
-----------

.. lua:class:: uielement

    :lua:class:`uielement` is the base class for all UI elements.

    All of the methods listed below can be called on any UI element.

    .. important::

        If you receive a Lua error with the message:

            Argument 1 is not a UI Element. (not userdata)

        Ensure that you are calling the method as a class method, not a function.
        I.e., ``element:method()`` and not ``element.method()``

    .. include:: /docs/_include/uielement.rst
*/
#[doc(hidden)]
unsafe extern "C" fn element_gc(l: &lua_State) -> i32 {
    let e = unsafe { checkelement(l, 1) };

    // lua::getfield(l, 1, "__name");

    // let t = lua::tostring(l, -1);

    // debug!("Dropping {} ({})",t, Arc::strong_count(&e));

    // lua::pop(l, 1);

    // strong count is decremented here
    let _: Arc<ui::Element> = ManuallyDrop::into_inner(e);

    return 0;
}


#[doc(hidden)]
unsafe extern "C" fn element_x(l: &lua_State) -> i32 {
    let e = unsafe { checkelement(l, 1) };

    if lua::gettop(l) == 2 {
        let newval = unsafe { lua::L::checkinteger(l, 2) };

        e.set_x(newval);
    }

    lua::pushinteger(l, e.get_x());

    return 1;
}

#[doc(hidden)]
unsafe extern "C" fn element_y(l: &lua_State) -> i32 {
    let e = unsafe { checkelement(l, 1) };

    if lua::gettop(l) == 2 {
        let newval = unsafe { lua::L::checkinteger(l, 2) };

        e.set_y(newval);
    }

    lua::pushinteger(l, e.get_y());

    return 1;
}


#[doc(hidden)]
unsafe extern "C" fn element_width(l: &lua_State) -> i32 {
    let e = unsafe { checkelement(l, 1) };

    if lua::gettop(l) == 2 {
        let w = unsafe { lua::L::checkinteger(l, 2) };
        e.set_width(w);
        return 0;
    }

    lua::pushinteger(l, e.get_width());

    return 1;
}

#[doc(hidden)]
unsafe extern "C" fn element_height(l: &lua_State) -> i32 {
    let e = unsafe { checkelement(l, 1) };

    if lua::gettop(l) == 2 {
        let h = unsafe { lua::L::checkinteger(l, 2) };
        e.set_height(h);

        return 0;
    }

    lua::pushinteger(l, e.get_height());

    return 1;
}

#[doc(hidden)]
unsafe extern "C" fn element_bg_color(l: &lua_State) -> i32 {
    let e = unsafe { checkelement(l, 1) };

    if lua::gettop(l) == 2 {
        let color = unsafe { checkcolor(l, 2) };
        e.set_bg_color(color);
    }

    lua::pushinteger(l, e.get_bg_color().into());

    return 1;
}

/*** RST
.. _colors:

Colors
------

All colors in EG-Overlay are represented by 32bit integers in RGBA format. This
may sound complicated, but it is actually convenient for module authors since
colors can be specified in hex format, similar to CSS.

For example, red at 100% opacity is ``0xFF0000FF``, green is ``0x00FF00FF``,
and blue is ``0x0000FFFF``.

.. seealso::

    Standard/default colors can be retrieved from :lua:func:`color`
*/
