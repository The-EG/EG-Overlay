// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT

/*** RST
Button Elements
===============

.. lua:module:: ui

Buttons provide an input element that a user can click on to perform an action
or click on to toggle state (on/off).

Button elements have a single child element, but layout elements, such as
:doc:`boxes <../uibox/lua>` can be used to create complex content.

New buttons can be created with the :lua:func:`button` function in the
:lua:mod:`ui` module.

Checkboxes
----------

Checkboxes are a specialized version of a button that displays a toggle (on or
off) state.

Checkboxes can be created with the :lua:func:`checkbox` function.

Functions
---------
*/

#[allow(unused_imports)]
use crate::logging::{debug, info, warn, error};

use crate::overlay::lua::{luawarn, luaerror};

use std::sync::Arc;
use std::mem::ManuallyDrop;

use std::collections::HashSet;

use crate::lua;
use crate::lua::lua_State;
use crate::lua::luaL_Reg;
use crate::lua::luaL_Reg_list;

use crate::ui;
use crate::ui::button;

const BUTTON_METATABLE_NAME: &str = "ui::Button";

const UI_MOD_FUNCS: &[luaL_Reg] = luaL_Reg_list! {
    c"button"  , new_button,
    c"checkbox", new_checkbox,
};

const BUTTON_FUNCS: &[luaL_Reg] = luaL_Reg_list! {
    c"child"              , child,
    c"addeventhandler"    , add_event_handler,
    c"removeeventhandler" , remove_event_handler,
    c"bghover"            , background_hover,
    c"bghighlight"        , background_highlight,
    c"border"             , border,
    c"borderwidth"        , border_width,
    c"checkstate"         , check_state,
};

pub fn register_module_functions(l: &lua_State) {
    lua::L::setfuncs(l, UI_MOD_FUNCS, 0);
}

/*** RST
.. lua:function:: button()

    Creates a new :lua:class:`uibutton`.

    :rtype: uibutton

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn new_button(l: &lua_State) -> i32 {
    let btn = button::Button::new();

    ui::lua::pushelement(l, &btn, BUTTON_METATABLE_NAME, Some(BUTTON_FUNCS));

    return 1;
}

/*** RST
.. lua:function:: checkbox(size)

    Creates a new :lua:class:`uibutton` that is specialized as a checkbox.

    :param integer size: The width/height of the checkbox.

    .. note::

        A checkbox button can't have a child element and will send two additional
        events to event handlers:

        - ``toggle-on``
        - ``toggle-off``

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn new_checkbox(l: &lua_State) -> i32 {
    let size = unsafe { lua::L::checkinteger(l, 1) };

    let btn = button::Button::new();

    let mut inner = btn.as_button().unwrap().inner.lock().unwrap();

    inner.is_checkbox = true;
    inner.min_width = size;
    inner.min_height = size;

    ui::lua::pushelement(l, &btn, BUTTON_METATABLE_NAME, Some(BUTTON_FUNCS));

    return 1;
}


/*** RST
Classes
-------

.. lua:class:: uibutton

    A button has a single child element and can be monitored for mouse events.

*/

/*** RST
    .. lua:method:: child(element)

        :param uielement element:

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn child(l: &lua_State) -> i32 {
    let btne = unsafe { ui::lua::checkelement(l, 1) };
    let c = unsafe { ui::lua::checkelement(l, 2) };

    let btn = unsafe { checkbutton(l, &btne) };

    if btn.inner.lock().unwrap().is_checkbox {
        luaerror!(l, "Checkbox button can't have a child set.");
        return 0;
    }

    btn.inner.lock().unwrap().child = Some((*c).clone());

    return 0;
}

/*** RST
    .. lua:method:: addeventhandler(handler[, event1, event2, ...])

        :param function handler:
        :param string events: (Optional) Name of events this handler will receive.
        :rtype: integer

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn add_event_handler(l: &lua_State) -> i32 {
    if lua::gettop(l) < 2 || lua::luatype(l, 2) != lua::LuaType::LUA_TFUNCTION {
        lua::pushstring(l, "button:addeventhandler argument #1 must be a Lua function.");
        return unsafe { lua::error(l) };
    }

    let btne = unsafe { ui::lua::checkelement(l, 1) };
    let btn = unsafe { checkbutton(l, &btne) };

    lua::pushvalue(l, 2);

    let ehref = lua::L::ref_(l, lua::LUA_REGISTRYINDEX);

    let mut events: HashSet<String> = HashSet::new();

    for i in 3..(lua::gettop(l)+1) {
        if let Some(e) = lua::tostring(l, i) {
            events.insert(e);
        } else {
            luaerror!(l, "Event names must be a string.");
        }
    }

    if events.len() == 0 {
        luawarn!(l, "No event types sepcified for button event handler, using default.");
        events.insert(String::from("click-left"));
    }

    _ = btn.inner.lock().unwrap().event_handlers.insert(ehref, events);

    lua::pushinteger(l, ehref);

    return 1;
}

/*** RST
    .. lua:method:: removeevnthandler(handlerid)

        :param integer handlerid: An event handler ID returned from :lua:meth:`addeventhandler`

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn remove_event_handler(l: &lua_State) -> i32 {
    let btne = unsafe { ui::lua::checkelement(l, 1) };
    let btn = unsafe { checkbutton(l, &btne) };
    let ehref = unsafe { lua::L::checkinteger(l, 2) };

    if btn.inner.lock().unwrap().event_handlers.remove(&ehref).is_none() {
        warn!("Button didn't have event handler {}", ehref);
    }

    lua::L::unref(l, lua::LUA_REGISTRYINDEX, ehref);

    return 0;
}

/*** RST
    .. lua:method:: bghover(color)

        Set the color drawn for the button's hovered background.

        :param integer color:

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn background_hover(l: &lua_State) -> i32 {
    let btne = unsafe { ui::lua::checkelement(l, 1) };
    let btn = unsafe { checkbutton(l, &btne) };
    let color = ui::Color::from(unsafe { lua::L::checkinteger(l, 2) });

    btn.inner.lock().unwrap().bg_hover = color;

    return 0;
}

/*** RST
    .. lua:method:: bghighlight(color)

        Set the color drawn for the button's highlighted background.

        :param integer color:

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn background_highlight(l: &lua_State) -> i32 {
    let btne = unsafe { ui::lua::checkelement(l, 1) };
    let btn = unsafe { checkbutton(l, &btne) };
    let color = ui::Color::from(unsafe { lua::L::checkinteger(l, 2) });

    btn.inner.lock().unwrap().bg_highlight = color;

    return 0;
}

/*** RST
    .. lua:method:: border(color)

        Set the color drawn for the button's border.

        :param integer color:

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn border(l: &lua_State) -> i32 {
    let btne = unsafe { ui::lua::checkelement(l, 1) };
    let btn = unsafe { checkbutton(l, &btne) };
    let color = ui::Color::from(unsafe { lua::L::checkinteger(l, 2) });

    btn.inner.lock().unwrap().border = color;

    return 0;
}

/*** RST
    .. lua:method:: borderwidth(pixels)

        Set the border thickness.

        :param integer pixels:

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn border_width(l: &lua_State) -> i32 {
    let btne = unsafe { ui::lua::checkelement(l, 1) };
    let btn = unsafe { checkbutton(l, &btne) };

    let pixels = unsafe { lua::L::checkinteger(l, 2) };

    btn.inner.lock().unwrap().border_width = pixels;

    return 0;
}

/*** RST
    .. lua:method:: checkstate([value])

        Get or set the state of this button if it is a checkbox.

        :param boolean value: (Optional) the new state to set.

        :rtype: boolean

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn check_state(l: &lua_State) -> i32 {
    let btne = unsafe { ui::lua::checkelement(l, 1) };
    let btn = unsafe { checkbutton(l, &btne) };

    if !btn.inner.lock().unwrap().is_checkbox {
        luaerror!(l, "Button is not a checkbox.");

        return 0;
    }

    if lua::gettop(l) >= 2 {
        let value = lua::toboolean(l, 2);

        btn.inner.lock().unwrap().toggle_state = value;
    }

    let ret = btn.inner.lock().unwrap().toggle_state;

    lua::pushboolean(l, ret);

    return 1;
}

/*** RST
    .. note::

        The following methods are inherited from :lua:class:`uielement`


    .. include:: /docs/_include/uielement.rst
*/


unsafe fn checkbutton<'a>(l: &lua_State, element: &'a ManuallyDrop<Arc<ui::Element>>) -> &'a button::Button {
    if let Some(b) = element.as_button() { b }
    else {
        lua::pushstring(l, "element is not a button.");
        unsafe { _ = lua::error(l); }
        panic!("element is not a button.");
    }
}

