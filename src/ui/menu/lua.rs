// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT

/*** RST
Menus
=====

.. lua:currentmodule:: eg-overlay-ui

Menus are a top level element that display a list of items. They are
typically used to provide contextual actions, ie. a context or 'right click' menu.

Menus function much like :doc:`boxes <../uibox/lua>`, except they can only have
:lua:class:`uimenuitem` as children.

New menus can be created with the :lua:func:`menu` function in the
:lua:mod:`eg-overlay-ui` module.

New menu items can be created with the following functions:

* :lua:func:`menuitem`
* :lua:func:`textmenuitem`
* :lua:func:`separatormenuitem`

Functions
---------
*/
use std::sync::{Arc};
use std::mem::ManuallyDrop;

use std::collections::HashSet;

use crate::overlay::lua::luawarn;

use crate::lua;
use crate::lua::lua_State;
use crate::lua::luaL_Reg;
use crate::lua::luaL_Reg_list;

use crate::ui;
use crate::ui::menu::{Menu, MenuItem};

const UI_MOD_FUNCS: &[luaL_Reg] = luaL_Reg_list!{
    c"menu"             , new_menu,
    c"menuitem"         , new_menu_item,
    c"textmenuitem"     , new_text_menu_item,
    c"separatormenuitem", new_sep_menu_item,
};

pub fn register_module_functions(l: &lua_State) {
    lua::L::setfuncs(l, UI_MOD_FUNCS, 0);
}

const MENU_METATABLE_NAME: &str = "ui::Menu";

const MENU_FUNCS: &[luaL_Reg] = luaL_Reg_list!{
    c"pushfront"   , push_front,
    c"pushback"    , push_back,
    c"popfront"    , pop_front,
    c"popback"     , pop_back,
    c"insertbefore", insert_before,
    c"insertafter" , insert_after,
    c"removeitem"  , remove_item,
    c"show"        , show,
    c"hide"        , hide,
};

unsafe fn checkmenu<'a>(l: &lua_State, element: &'a ManuallyDrop<Arc<ui::Element>>) -> &'a Menu {
    if let Some(m) =element.as_menu() {
        &m
    } else {
        lua::pushstring(l, "element not a menu.");
        unsafe { lua::error(l); }
        panic!("not a menu.");
    }
}

/*** RST
.. lua:function:: menu()
    
    Create a new :lua:class:`uimenu`.

    :rtype: uimenu

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn new_menu(l: &lua_State) -> i32 {
    let menu = Menu::new();

    ui::lua::pushelement(l, &menu, MENU_METATABLE_NAME, Some(MENU_FUNCS));

    return 1;
}

/*** RST
.. lua:function:: menuitem()
    
    Create a new :lua:class:`uimenuitem`.

    :rtype: uimenuitem

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn new_menu_item(l: &lua_State) -> i32 {
    let mi = MenuItem::new();

    ui::lua::pushelement(l, &mi, MENUITEM_METATABLE_NAME, Some(MENUITEM_FUNCS));

    return 1;
}

/*** RST
.. lua:function:: textmenuitem(text, color, font)

    Create a new menu item containing a :lua:class:`uitext`.

    :param string text:
    :param integer color:
    :param uifont font:
    :rtype: uimenuitem

    .. seealso::

        ``text``, ``color``, and ``font`` are passed directly to :lua:func:`text`.

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn new_text_menu_item(l: &lua_State) -> i32 {
    // check args first, if the error occurs in the call below it will cause a
    // panic instead of an error
    unsafe { lua::L::checkstring(l, 1); }
    unsafe { lua::L::checkinteger(l, 2); }
    unsafe { ui::font::lua::checkfont(l, 3); }

    // local ui = require 'eg-overlay-ui'
    lua::getglobal(l, "require");
    lua::pushstring(l, "eg-overlay-ui");
    lua::call(l, 1, 1);

    let ui = lua::gettop(l);

    // local text = ui.text(<param 1>, <param 2>, <param 3>)
    lua::getfield(l, -1, "text");
    // copy the text values directly
    lua::pushvalue(l, 1);
    lua::pushvalue(l, 2);
    lua::pushvalue(l, 3); 
    lua::call(l, 3, 1);

    let text = lua::gettop(l);

    // mi = ui.menuitem()
    unsafe { new_menu_item(l); }

    // mi:element(text)
    lua::getfield(l, -1, "element");
    lua::pushvalue(l, -2);
    lua::pushvalue(l, text);
    lua::call(l, 2, 0);

    lua::remove(l, text);
    lua::remove(l, ui);

    return 1;
}

/*** RST
.. lua:function:: separatormenuitem(orientation)

    Create a new :lua:class:`uimenuitem` with a :lua:class:`uiseparator`.

    :param string orientation:
    :rtype: uimenuitem

    .. seealso::
    
        ``orientation`` is passed directly to :lua:func:`separator`.

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn new_sep_menu_item(l: &lua_State) -> i32 {
    // as above, check args first
    unsafe { lua::L::checkstring(l, 1); }

    // local ui = require 'eg-overlay-ui'
    lua::getglobal(l, "require");
    lua::pushstring(l, "eg-overlay-ui");
    lua::call(l, 1, 1);

    let ui = lua::gettop(l);

    // local sep = ui.separator(<param 1>)
    lua::getfield(l, -1, "separator");
    lua::pushvalue(l, 1);
    lua::call(l, 1, 1);

    let sep = lua::gettop(l);

    // mi = ui.menuitem()
    unsafe { new_menu_item(l); }

    // mi:element(sep)
    lua::getfield(l, -1, "element");
    lua::pushvalue(l, -2);
    lua::pushvalue(l, sep);
    lua::call(l, 2, 0);

    // mi:enabled(false)
    lua::getfield(l, -1, "enabled");
    lua::pushvalue(l, -2);
    lua::pushboolean(l, false);
    lua::call(l, 2, 0);

    lua::remove(l, sep);
    lua::remove(l, ui);

    return 1;
}


/*** RST
Classes
-------

.. lua:class:: uimenu

    .. lua:method:: pushfront(item)

        Add the given :lua:class:`uimenuitem` to the beginning of the menu.

        :param uimenuitem item:

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn push_front(l: &lua_State) -> i32 {
    let menu_e = unsafe { ui::lua::checkelement(l, 1) };
    let menu = unsafe { checkmenu(l, &menu_e) };

    let item_e = unsafe { ui::lua::checkelement(l, 2) };
    let menu_item = unsafe { checkmenuitem(l, &item_e) };

    let inner = menu.inner.lock().unwrap();

    inner.itembox.as_box().unwrap().push_front(&item_e, ui::ElementAlignment::Fill, false);

    menu_item.inner.lock().unwrap().parent_menu = Arc::downgrade(&menu_e);
    
    return 0;
}

/*** RST
    .. lua:method:: pushback(item)

        Add the given :lua:class:`uimenuitem` to the end of the menu.

        :param uimenuitem item:

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn push_back(l: &lua_State) -> i32 {
    let menu_e = unsafe { ui::lua::checkelement(l, 1) };
    let menu = unsafe { checkmenu(l, &menu_e) };

    let item_e = unsafe { ui::lua::checkelement(l, 2) };
    let menu_item = unsafe { checkmenuitem(l, &item_e) };

    let inner = menu.inner.lock().unwrap();

    inner.itembox.as_box().unwrap().push_back(&item_e, ui::ElementAlignment::Fill, false);

    menu_item.inner.lock().unwrap().parent_menu = Arc::downgrade(&menu_e);

    return 0;
}

/*** RST
    .. lua:method:: popfront()

        Remove the first item from the menu.

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn pop_front(l: &lua_State) -> i32 {
    let menu_e = unsafe { ui::lua::checkelement(l, 1) };
    let menu = unsafe { checkmenu(l, &menu_e) };

    let inner = menu.inner.lock().unwrap();

    inner.itembox.as_box().unwrap().pop_front();
    
    return 0;
}

/*** RST
    .. lua:method:: popback()

        Remove the last item from the menu.

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn pop_back(l: &lua_State) -> i32 {
    let menu_e = unsafe { ui::lua::checkelement(l, 1) };
    let menu = unsafe { checkmenu(l, &menu_e) };
    
    let inner = menu.inner.lock().unwrap();
    inner.itembox.as_box().unwrap().pop_back();
    
    return 0;
}

/*** RST
    .. lua:method:: insertbefore(before, item)
    
        Insert ``item`` before item ``before``.

        If ``before`` is not in this menu, ``item`` is not added and this
        method will return ``false``. ``true`` is returned on success.

        :param uimenuitem before:
        :param uimenuitem item:
        :rtype: boolean

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn insert_before(l: &lua_State) -> i32 {
    let menu_e = unsafe { ui::lua::checkelement(l, 1) };
    let menu = unsafe { checkmenu(l, &menu_e) };

    let before_e = unsafe { ui::lua::checkelement(l, 2) };
    let _before_item = unsafe { checkmenuitem(l, &before_e) };

    let item_e = unsafe { ui::lua::checkelement(l, 3) };
    let item = unsafe { checkmenuitem(l, &item_e) };

    let inner = menu.inner.lock().unwrap();

    let r = inner.itembox.as_box().unwrap().insert_before(&before_e, &item_e, ui::ElementAlignment::Fill, false);

    if r {
        item.inner.lock().unwrap().parent_menu = Arc::downgrade(&menu_e);
        lua::pushboolean(l, true);
    } else {
        lua::pushboolean(l, false);
    }

    return 1;
}

/*** RST
    .. lua:method:: insertafter(after, item)
    
        Insert ``item`` after item ``after``.

        If ``after`` is not in this menu, ``item`` is not added and this
        method will return ``false``. ``true`` is returned on success.

        :param uimenuitem after:
        :param uimenuitem item:
        :rtype: boolean

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn insert_after(l: &lua_State) -> i32 {
    let menu_e = unsafe { ui::lua::checkelement(l, 1) };
    let menu = unsafe { checkmenu(l, &menu_e) };

    let after_e = unsafe { ui::lua::checkelement(l, 2) };
    let _after_item = unsafe { checkmenuitem(l, &after_e) };

    let item_e = unsafe { ui::lua::checkelement(l, 3) };
    let item = unsafe { checkmenuitem(l, &item_e) };

    let inner = menu.inner.lock().unwrap();

    let r = inner.itembox.as_box().unwrap().insert_after(&after_e, &item_e, ui::ElementAlignment::Fill, false);

    if r {
        item.inner.lock().unwrap().parent_menu = Arc::downgrade(&menu_e);
        lua::pushboolean(l, true);
    } else {
        lua::pushboolean(l, false);
    }

    return 1;
}


/*** RST
    .. lua:method:: removeitem(item)

        Remove the given :lua:class:`uimenuitem` from the menu.

        :param uimenuitem item:

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn remove_item(l: &lua_State) -> i32 {
    let menu_e = unsafe { ui::lua::checkelement(l, 1) };
    let menu = unsafe { checkmenu(l, &menu_e) };

    let item_e = unsafe { ui::lua::checkelement(l, 2) };
    let _menu_item = unsafe { checkmenuitem(l, &item_e) }; // we don't actually need it, just check that it is

    let inner = menu.inner.lock().unwrap();

    inner.itembox.as_box().unwrap().remove_item(&item_e);

    return 0;
}

/*** RST
    .. lua:method:: show([x, y])

        Show the menu, either at the given location or at the mouse cursor.

        :param integer x: (Optional)
        :param integer y: (Optional)

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn show(l: &lua_State) -> i32 {
    let menu_e = unsafe { ui::lua::checkelement(l, 1) };
    let menu = unsafe { checkmenu(l, &menu_e) };

    let x: i64;
    let y: i64;

    if lua::gettop(l) == 3 {
        x = unsafe { lua::L::checkinteger(l, 2) };
        y = unsafe { lua::L::checkinteger(l, 3) };
    } else if lua::gettop(l) == 1 {
        // x,y = require('eg-overlay-ui').mouseposition()
        lua::getglobal(l,"require");
        lua::pushstring(l, "eg-overlay-ui");
        lua::call(l, 1, 1);

        lua::getfield(l, -1, "mouseposition");
        lua::call(l, 0, 2);

        x = lua::tointeger(l, -2);
        y = lua::tointeger(l, -1);

        lua::pop(l, 3);
    } else {
        lua::pushstring(l, "menu:show takes either 0 or 2 arguments.");
        return unsafe { lua::error(l) };
    }

    menu.show(x, y, &menu_e);

    return 0;
}

/*** RST
    .. lua:method:: hide()

        Hide the menu.

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn hide(l: &lua_State) -> i32 {
    let menu_e = unsafe { ui::lua::checkelement(l, 1) };
    let menu = unsafe { checkmenu(l, &menu_e) };
    
    menu.hide(&menu_e);
    
    return 0;
}

/*** RST
    .. note::

        The following methods are inherited from :lua:class:`uielement`

    .. include:: /docs/_include/uielement.rst
*/

const MENUITEM_METATABLE_NAME: &str = "ui::MenuItem";

const MENUITEM_FUNCS: &[luaL_Reg] = luaL_Reg_list!{
    c"element"           , menuitem_element,
    c"enabled"           , menuitem_enabled,
    c"icon"              , menuitem_icon,
    c"submenu"           , menuitem_submenu,
    c"addeventhandler"   , menuitem_add_event_handler,
    c"removeeventhandler", menuitem_remove_event_handler,
};

unsafe fn checkmenuitem<'a>(l: &lua_State, element: &'a ManuallyDrop<Arc<ui::Element>>) -> &'a MenuItem {
    if let Some(mi) = element.as_menuitem() {
        &mi
    } else {
        lua::pushstring(l, "element not a menu item.");
        unsafe { lua::error(l); }
        panic!("not a menu item.");
    }
}


unsafe extern "C" fn menuitem_element(l: &lua_State) -> i32 {
    let mi_e = unsafe { ui::lua::checkelement(l, 1) };
    let mi = unsafe { checkmenuitem(l, &mi_e) };

    let e = unsafe { ui::lua::checkelement(l, 2) };

    mi.inner.lock().unwrap().element = Some((*e).clone());

    return 0;
}

/*** RST
.. lua:class:: uimenuitem

    A item within a :lua:class:`uimenu`.

    Menu items are made up of 3 pieces:

    .. code-block:: text
    
        +--------+---------+------------+
        | (icon) | (child) | (sub-menu) |
        +--------+---------+------------+

    The icon is a text value displayed using the default icon font. This can
    be used to indicate a status such as on/off or to give additional context or
    a visual hint to the menu item.

    The child is any valid UI element; most commonly the child will be a :lua:class:`uitext`.

    The sub-menu item will only be shown if the item contains a sub-menu. This
    gives the user an indication that the sub-menu exists and will be shown
    when the item is hovered.

    .. lua:method:: enabled(value)

        Set if this menu item is enabled or not.

        A disabled menu item does not react or send mouse events.

        :param boolean value:

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn menuitem_enabled(l: &lua_State) -> i32 {
    let mi_e = unsafe { ui::lua::checkelement(l, 1) };
    let mi = unsafe { checkmenuitem(l, &mi_e) };

    mi.inner.lock().unwrap().enabled = lua::toboolean(l, 2);

    return 0;
}

/*** RST
    .. lua:method:: icon(codepoint)

        Set the icon to be shown on this menu item.

        :param string codepoint: codepoint/string value of the icon

        .. seealso::

            :lua:func:`iconcodepoint` can be used to lookup an icon codepoint.

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn menuitem_icon(l: &lua_State) -> i32 {
    let mi_e = unsafe { ui::lua::checkelement(l, 1) };
    let mi = unsafe { checkmenuitem(l, &mi_e) };

    let icon = unsafe { lua::L::checkstring(l, 2) };

    mi.inner.lock().unwrap().icon_text = String::from(icon);

    return 0;
}

/*** RST
    .. lua:method:: submenu(menu)

        :param uimenu menu:

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn menuitem_submenu(l: &lua_State) -> i32 {
    let mi_e = unsafe { ui::lua::checkelement(l, 1) };
    let mi = unsafe { checkmenuitem(l, &mi_e) };

    let menu_e = unsafe { ui::lua::checkelement(l, 2) };
    let _menu = unsafe { checkmenu(l, &menu_e) };

    mi.inner.lock().unwrap().child_menu = Some((*menu_e).clone());

    return 0;
}

/*** RST
    .. lua:method:: addeventhandler(handler[, event1, event2, ...])

        Add an event handler.

        If no event types are specified, 'click-left' will be used.

        :param function handler:
        :param string event1: (Optional)
        :rtype: integer

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn menuitem_add_event_handler(l: &lua_State) -> i32 {
    if lua::gettop(l) < 2 || lua::luatype(l, 2) != lua::LuaType::LUA_TFUNCTION {
        lua::pushstring(l, "menuitem:addeventhandler argument #2 must be a Lua function.");
        return unsafe { lua::error(l) };
    }

    let mi_e = unsafe { ui::lua::checkelement(l, 1) };
    let mi = unsafe { checkmenuitem(l, &mi_e) };

    lua::pushvalue(l, 2);

    let ehref = lua::L::ref_(l, lua::LUA_REGISTRYINDEX);

    let mut events: HashSet<String> = HashSet::new();

    for i in 3..(lua::gettop(l)+1) {
        let e = lua::tostring(l, i);
        events.insert(String::from(e));
    }

    if events.len() == 0 {
        luawarn!(l, "No event types specified for menuitem event handler, using default.");
        events.insert(String::from("click-left"));
    }

    mi.inner.lock().unwrap().event_handlers.insert(ehref, events);

    lua::pushinteger(l, ehref);

    return 1;
}

/*** RST
    .. lua:method:: removeeventhandler(handlerid)

        Remove an event handler added with :lua:meth:`addeventhandler`.

        :param integer handlerid:

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn menuitem_remove_event_handler(l: &lua_State) -> i32 {
    let mi_e = unsafe { ui::lua::checkelement(l, 1) };
    let mi = unsafe { checkmenuitem(l, &mi_e) };
    let ehref = unsafe { lua::L::checkinteger(l, 2) };

    if mi.inner.lock().unwrap().event_handlers.remove(&ehref).is_none() {
        luawarn!(l, "menuitem didn't have event handler {}", ehref);
    }

    lua::L::unref(l, lua::LUA_REGISTRYINDEX, ehref);

    return 0;
}

/*** RST
    .. note::

        The following methods are inherited from :lua:class:`uielement`


    .. include:: /docs/_include/uielement.rst
*/

