// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT

/*** RST
Windows
=======

.. lua:currentmodule:: ui

A window is the top level UI element that contains all other elements. Most
modules that display some sort of UI to the user will have at least one window.

Windows can only have a single child element. In most cases this child will be
a :doc:`box <../uibox/lua>` or :doc:`grid <../grid/lua>`.

New windows are created with the :lua:func:`window` function in the
:lua:mod:`ui` module (see below).

Functions
---------
*/
#[allow(unused_imports)]
use crate::logging::{debug, info, warn, error};

use std::sync::{Arc};
use std::mem::ManuallyDrop;

use crate::lua;
use crate::lua::lua_State;
use crate::lua::luaL_Reg;
use crate::lua::luaL_Reg_list;

use crate::ui;
use crate::overlay;
use crate::ui::window::Window;

const WIN_METATABLE_NAME: &str = "ui::Window";

const UI_MOD_FUNCS: &[luaL_Reg] = luaL_Reg_list!{
    c"window", new_window
};

pub fn register_module_functions(l: &lua_State) {
    lua::L::setfuncs(l, UI_MOD_FUNCS, 0);
}

const WIN_FUNCS: &[luaL_Reg] = luaL_Reg_list!{
    c"caption"  , caption,
    c"child"    , child,
    c"show"     , show,
    c"hide"     , hide,
    c"resizable", resizable,
    c"settings" , settings,
    c"position" , position,
    c"titlebar" , titlebar,
};

unsafe fn checkwindow<'a>(l: &lua_State, element: &'a ManuallyDrop<Arc<ui::Element>>) -> &'a Window {
    if let Some(w) = element.as_window() {
        w
    } else {
        lua::pushstring(l, "element not a window.");
        unsafe { _ = lua::error(l); }
        panic!("not a window");
    }
}


/*** RST
.. lua:function:: window(caption)

    Create a new :lua:class:`uiwindow`.

    :param string caption: The window title.

    :return: A new window.
    :rtype: uiwindow

    .. code-block:: lua
        :caption: Example

        local ui = require 'ui'

        local win = ui.window("My Module")

        win:show()

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn new_window(l: &lua_State) -> i32 {
    let caption = unsafe { lua::L::checkstring(l,1) };

    let win = Window::new(&caption);
    
    ui::lua::pushelement(l, &win, WIN_METATABLE_NAME, Some(WIN_FUNCS));

    return 1;
}

/*** RST
classes
-------

.. lua:class:: uiwindow

    A top-level window element. A window can only have a single child, which
    should be a layout container such as a :lua:class:`uibox`.

    .. lua:method:: caption([newcaption])
        
        Set or return the window title.

        :param string newcaption: (Optional) The new window title.
        :return: The current window title.
        :rtype: string

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn caption(l: &lua_State) -> i32 {
    let e = unsafe { ui::lua::checkelement(l, 1) };
    let win = unsafe { checkwindow(l, &e) };

    if lua::gettop(l) >= 2 {
        let newcaption = unsafe { lua::L::checkstring(l, 2) };

        win.win.lock().unwrap().caption = String::from(newcaption);            
    }

    lua::pushstring(l, &*win.win.lock().unwrap().caption);

    return 1;
}

/*** RST
    .. lua:method:: child(newchild)

        Set the window's child. This can be any UI element, but in most cases
        this will be a layout container such as a box.

        .. note::
            
            To clear the child, pass ``nil``.

        :param uielement newchild:

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn child(l: &lua_State) -> i32 {
    let e = unsafe { ui::lua::checkelement(l, 1) };
    let win = unsafe { checkwindow(l, &e) };

    let child: Option<Arc<ui::Element>>;

    if lua::luatype(l,2) == lua::LuaType::LUA_TNIL {
        child = None;
    } else {
        let c = unsafe { ui::lua::checkelement(l, 2) };

        child = Some((*c).clone());
    }

    win.win.lock().unwrap().child = child;

    return 0;
}


/*** RST
    .. lua:method:: show()

        Show the window.

        If the window is already visible this function has no effect.

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn show(l: &lua_State) -> i32 {
    let e = unsafe { ui::lua::checkelement(l, 1) };

    overlay::ui().add_top_level_element(&e);

    return 0;
}

/*** RST
    .. lua:method:: hide()

        Hide the window.

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn hide(l: &lua_State) -> i32 {
    let e = unsafe { ui::lua::checkelement(l, 1) };

    overlay::ui().remove_top_level_element(&e);

    return 0;
}

/*** RST
    .. lua:method:: settings(settings, path)

        Bind this window to the given settings store.

        This allows a window's position and size to be persisted between overlay
        sessions.

        :param settingsstore settings:
        :param string path: The settings path/key to store the window settings.

        ``path`` is the path within the settings store where the following values
        will be read and stored:

        +--------+-------------+
        | Value  | Description |
        +========+=============+
        | x      | Position X  |
        +--------+-------------+
        | y      | Position Y  |
        +--------+-------------+
        | width  | Width       |
        +--------+-------------+
        | height | Height      |
        +--------+-------------+

        .. warning::

            ``path`` should contain (default) values for the above values before
            this method is called.

        .. code-block:: lua
            :caption: Example

            local overlay = require 'overlay'
            local ui = require 'ui'

            local settings = overlay.settings('my-module.lua')

            settings:setdefault('window.x', 50)
            settings:setdefault('window.y', 50)
            settings:setdefault('window.width', 400)
            settings:setdefault('window.height', 200)

            local win = ui.window('My Module')
            win:settings(settings, 'window')

            win:show()
       
        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn settings(l: &lua_State) -> i32 {
    let e = unsafe { ui::lua::checkelement(l, 1) };
    let win = unsafe { checkwindow(l, &e) };

    let s = unsafe { crate::settings::lua::checksettings(l, 2) };
    let path = unsafe { lua::L::checkstring(l, 3) };

    let mut data = win.win.lock().unwrap();
    data.settings = Some((*s).clone());
    data.settings_path = Some(path.to_string());

    data.update_from_settings();

    return 0;
}

/*** RST
    .. lua:method:: resizable(value)

        Set if this window can be resized by the user or not.

        A resizable window can be changed by dragging the left, right, or
        bottom borders.

        :param boolean value:

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn resizable(l: &lua_State) -> i32 {
    let e = unsafe { ui::lua::checkelement(l, 1) };
    let win = unsafe { checkwindow(l, &e) };

    win.win.lock().unwrap().resizable = lua::toboolean(l, 2);

    return 0;
}

/*** RST
    .. lua:method:: position(x, y)

        Set the window position.

        :param integer x:
        :param integer y:

        .. note::

            This accomplishes the same as calling :lua:meth:`x` and :lua:meth:`y`
            but in a single call.

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn position(l: &lua_State) -> i32 {
    let e = unsafe { ui::lua::checkelement(l, 1) };
    let win = unsafe { checkwindow(l, &e) };

    let x = unsafe { lua::L::checkinteger(l, 2) };
    let y = unsafe { lua::L::checkinteger(l, 3) };

    let mut w = win.win.lock().unwrap();
    w.x = x;
    w.y = y;

    return 0;
}

/*** RST
    .. lua:method:: titlebar(show)

        Set if the window should show a titlebar with a caption/title.

        If no titlebar is shown, the window will not have a caption displayed
        either.

        :param boolean show:

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn titlebar(l: &lua_State) -> i32 {
    let e = unsafe { ui::lua::checkelement(l, 1) };
    let win = unsafe { checkwindow(l, &e) };

    let show = lua::toboolean(l, 2);

    win.win.lock().unwrap().show_titlebar = show;

    return 0;
}


/*** RST
    
    .. note::

        The following methods are inherited from :lua:class:`uielement`
    
    .. include:: /docs/_include/uielement.rst
*/
