// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT

/*** RST
Text Entry Element
==================

.. lua:currentmodule:: eg-overlay-ui

An entry element provides allows users to enter a single line of text, optionally
display a hint when no value has been provided.

A new entry can be created with the :lua:func:`entry` function in the
:lua:mod:`eg-overlay-ui`.

Functions
---------
*/
#[allow(unused_imports)]
use crate::logging::{debug, info, warn, error};

use crate::overlay::lua::luawarn;

use std::sync::Arc;
use std::mem::ManuallyDrop;

use std::collections::HashSet;

use crate::lua;
use crate::lua::lua_State;
use crate::lua::luaL_Reg;
use crate::lua::luaL_Reg_list;

use crate::ui;
use crate::ui::entry::Entry;

const ENTRY_METATABLE_NAME: &str = "ui::Entry";

const UI_MOD_FUNCS: &[luaL_Reg] = luaL_Reg_list!{
    c"entry", new_entry,
};

const ENTRY_FUNCS: &[luaL_Reg] = luaL_Reg_list!{
    c"text"              , text,
    c"hint"              , hint,
    c"prefwidth"         , pref_width,
    c"addeventhandler"   , add_event_handler,
    c"removeeventhandler", remove_event_handler,
    c"readonly"          , read_only,
};

pub fn register_module_functions(l: &lua_State) {
    lua::L::setfuncs(l, UI_MOD_FUNCS, 0);
}


/*** RST
.. lua:function:: entry(font)

    Create a new :lua:class:`uientry`.

    :param uifont font:
    :rtype: uientry

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn new_entry(l: &lua_State) -> i32 {
    let font = unsafe { ui::font::lua::checkfont(l, 1) };

    let e = Entry::new(&font);

    ui::lua::pushelement(l, &e, ENTRY_METATABLE_NAME, Some(ENTRY_FUNCS));

    return 1;
}

unsafe fn checkentry<'a>(l: &lua_State, element: &'a ManuallyDrop<Arc<ui::Element>>) -> &'a Entry {
    if let Some(e) = element.as_entry() { e }
    else {
        lua::pushstring(l, "element is not an entry.");
        unsafe { lua::error(l); }
        panic!("element is not an entry.");
    }
}

/*** RST
Classes
-------

.. lua:class:: uientry

    .. lua:method:: text([newtext])

        Get or set the value of this text entry.

        :param string newtext: (Optional)
        :rtype: string

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn text(l: &lua_State) -> i32 {
    let ele = unsafe { ui::lua::checkelement(l, 1) };
    let entry = unsafe { checkentry(l, &ele) };

    if lua::gettop(l) >= 2 {
        let t = unsafe { lua::L::checkstring(l, 2) };
            
        let mut inner = entry.inner.lock().unwrap();
        inner.text = String::from(t);
        inner.update_caret_x();

        return 0;
    } else {
        lua::pushstring(l, &entry.inner.lock().unwrap().text);

        return 1;
    }
}

/*** RST
    .. lua:method:: hint(text)

        Set a text value to be shown in the entry when it is empty.

        This is typically a hint or example that can help a user understand what
        to enter into the entry.

        :param string text:

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn hint(l: &lua_State) -> i32 {
    let ele = unsafe { ui::lua::checkelement(l, 1) };
    let entry = unsafe { checkentry(l, &ele) };

    let hint = unsafe { lua::L::checkstring(l, 2) };

    entry.inner.lock().unwrap().hint = Some(String::from(hint));

    return 0;
}

/*** RST
    .. lua:method:: prefwidth(pixels)
    
        Set the preferred width of the text box.

        :param integer pixels:

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn pref_width(l: &lua_State) -> i32 {
    let ele = unsafe { ui::lua::checkelement(l, 1) };
    let entry = unsafe { checkentry(l, &ele) };
    let w = unsafe { lua::L::checkinteger(l, 2) };

    entry.inner.lock().unwrap().pref_width = w;

    return 0;
}

/*** RST
    .. lua:method:: addeventhandler(handler, event1[, event2, ...])

        Add an event handler for the given events.

        Multiple events can be specified, but at least one is required.

        In addition to the standard element events, an entry will also send
        key events. Key events are named in a ``{mod1}-{mod2}-{key}-{updown}`` format.
        For example: ``ctrl-v-down`` or ``ctrl-shift-a-up``, etc.

        An event handler added with this method can be removed later with
        :lua:meth:`removeeventhandler`.

        :param function handler:
        :param string event1:
        :rtype: integer

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn add_event_handler(l: &lua_State) -> i32 {
    if lua::gettop(l) < 2 || lua::luatype(l, 2) != lua::LuaType::LUA_TFUNCTION {
        lua::pushstring(l, "entry:addeventhandler argument #2 must be a Lua function.");
        return unsafe { lua::error(l) };
    }

    let ele = unsafe { ui::lua::checkelement(l, 1) };
    let entry = unsafe { checkentry(l, &ele) };

    let mut events: HashSet<String> = HashSet::new();

    for i in 3..(lua::gettop(l)+1) {
        let e = lua::tostring(l, i);
        events.insert(String::from(e));
    }

    if events.len() == 0 {
        luawarn!(l, "No event types sepcified for entry event handler, ignoring.");

        return 0;
    }

    lua::pushvalue(l, 2);
    let ehref = lua::L::ref_(l, lua::LUA_REGISTRYINDEX);

    _ = entry.inner.lock().unwrap().event_handlers.insert(ehref, events);

    lua::pushinteger(l, ehref);

    return 1;
}

/*** RST
    .. lua:method:: removeeventhandler(handlerid)

        Removes an event handler.

        :param integer handlerid: An event handler id returned by :lua:meth:`addeventhandler`

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn remove_event_handler(l: &lua_State) -> i32 {
    let ele = unsafe { ui::lua::checkelement(l, 1) };
    let entry = unsafe { checkentry(l, &ele) };
    let ehref = unsafe { lua::L::checkinteger(l, 2) };

    if entry.inner.lock().unwrap().event_handlers.remove(&ehref).is_none() {
        luawarn!(l, "Entry didn't have event handler {}", ehref);
    }

    lua::L::unref(l, lua::LUA_REGISTRYINDEX, ehref);

    return 0;
}

/*** RST
    .. lua:method:: readonly(value)

        Sets if this entry should be read only or not.

        A read only entry does not receive keyboard focus or events and the
        value can not be edited by the user.

        :param boolean value:

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn read_only(l: &lua_State) -> i32 {
    let ele = unsafe { ui::lua::checkelement(l, 1) };
    let entry = unsafe { checkentry(l, &ele) };
    let val = lua::toboolean(l, 2);

    entry.inner.lock().unwrap().readonly = val;

    return 0;
}

/*** RST
    .. note::

        The following methods are inherited from :lua:class:`uielement`
    
    .. include:: /docs/_include/uielement.rst
*/
