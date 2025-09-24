// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT

/*** RST
Text Elements
=============

.. lua:currentmodule:: ui

Text elements display text in a single font and color. Multiple text elements
can be combined with layout elements to create content with multiple fonts/colors.

A new text element can be created with the :lua:func:`text` function in the
:lua:mod:`ui` module.

.. seealso::

    See :doc:`../font/lua` for details on how EG-Overlay handles and renders
    fonts.

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
use crate::ui::text::Text;

const TEXT_METATABLE_NAME: &str = "ui::Text";

const UI_MOD_FUNCS: &[luaL_Reg] = luaL_Reg_list!{
    c"text", new_text
};

const TEXT_FUNCS: &[luaL_Reg] = luaL_Reg_list!{
    c"text", text
};

pub fn register_module_functions(l: &lua_State) {
    lua::L::setfuncs(l, UI_MOD_FUNCS, 0);
}

/*** RST
.. lua:function:: text(text_value, color, font)

    Create a new :lua:class:'uitext' element.

    :param string text: The text string to display.
    :param integer color: Text color. See :ref:`colors`.
    :param uifont font: The font to use to render the text. See :lua:class:`uifont`.

    :return: A Text element

    .. versionhistory::
        :0.0.1: Added
*/
unsafe extern "C" fn new_text(l: &lua_State) -> i32 {
    lua::checkargstring!(l, 1);
    lua::checkarginteger!(l, 2);

    let font = unsafe { ui::font::lua::checkfont(l, 3) };

    let text = lua::tostring(l, 1).unwrap();
    let color = ui::Color::from(lua::tointeger(l, 2) as u32);

    let t = ui::text::Text::new(&text, color, &font);

    ui::lua::pushelement(l, &t, TEXT_METATABLE_NAME, Some(TEXT_FUNCS));

    return 1;
}

/*** RST
Classes
-------

.. lua:class:: uitext

    A text element

    .. lua:method:: text([newtext])

        Get or set the text displayed in this text.

        :param string newtext: The new text to display.
        :returns: The current or new text.

        .. versionhistory::
            :0.2.0: Added
*/
unsafe extern "C" fn text(l: &lua_State) -> i32 {
    let e = unsafe { ui::lua::checkelement(l, 1) };

    let text = unsafe { checktext(l, &e) };

    if lua::gettop(l) >= 2 {
        let newtext = unsafe { lua::L::checkstring(l, 2) };

        let mut t = text.text.lock().unwrap();
        t.text = newtext.clone();
        t.update_text_size();

        lua::pushstring(l, &newtext);

        return 1;
    } else {
        lua::pushstring(l, &text.text.lock().unwrap().text);

        return 1;
    }
}

/*** RST
    .. note::

        The following methods are inherited from :lua:class:`uielement`

    .. include:: /docs/_include/uielement.rst
*/

unsafe fn checktext<'a>(l: &lua_State, element: &'a ManuallyDrop<Arc<ui::Element>>) -> &'a Text {
    if let Some(t) = element.as_text() { t }
    else {
        lua::pushstring(l, "element is not a text.");
        unsafe { _ = lua::error(l); }
        panic!("element is not a text.");
    }
}


