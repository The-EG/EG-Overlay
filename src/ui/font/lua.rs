// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT

/*** RST
Fonts
=====

.. lua:currentmodule:: ui

EG-Overlay uses FreeType2 to render font glyphs on screen. This means that any
font that FreeType2 supports can be used, **however, module authors are
encouraged to use the default fonts detailed below.**

Fonts can be accessed using the :lua:func:`getfont` function in the
:lua:mod:`ui` module.

Default Fonts
-------------

EG-Overlay comes with 4 default fonts:

- Regular and Italic: `Inter <https://github.com/rsms/inter>`_
- Monospace:  `Cascadia Code <https://github.com/microsoft/cascadia-code>`_
- Icon: `Google Material Design Icons <https://github.com/google/material-design-icons>`_

Each of the above fonts are initialized with default values and can be accessed
in a ``fonts`` table on the :lua:mod:`ui` module as ``regular``,
``monospace``, and ``icon``.

.. code-block:: lua
    :caption: Example

    local ui = require 'ui'

    local regfont = ui.fonts.regular
    local italfont = ui.fonts.italic
    local monofont = ui.fonts.monospace
    local iconfont = ui.fonts.icon

Functions
---------
*/
#[allow(unused_imports)]
use crate::logging::{debug, info, warn, error};

use crate::lua;
use crate::lua::lua_State;
use crate::lua::luaL_Reg;
use crate::lua::luaL_Reg_list;

use std::mem::ManuallyDrop;

use std::sync::Arc;

use crate::ui::font::Font;

#[doc(hidden)]
const FONT_METATABLE_NAME: &str = "ui::Font";

#[doc(hidden)]
const FONT_FUNCS: &[luaL_Reg] = luaL_Reg_list!{
    c"path"      , path,
    c"__gc"      , __gc,
    c"tosize"    , to_size,
    c"tosizeperc", to_size_perc,
};

/// Pushes a [Font] onto the stack.
///
/// This will increase the strong reference count of `font`.
pub fn pushfont(l: &lua_State, font: &Arc<Font>) {
    let font_ptr = Arc::into_raw(font.clone());

    let lua_font_ptr: *mut *const Font = unsafe {
        std::mem::transmute(lua::newuserdatauv(l, std::mem::size_of::<*const Font>(), 0))
    };

    unsafe { *lua_font_ptr = font_ptr; }

    if lua::L::newmetatable(l, FONT_METATABLE_NAME) {
        lua::pushvalue(l, -1);
        lua::setfield(l, -2, "__index");

        lua::L::setfuncs(l, FONT_FUNCS, 0);
    }
    lua::setmetatable(l, -2);
}

pub unsafe fn checkfont_ptr(l: &lua_State, ind: i32) -> *mut *const Font {
    unsafe {
        std::mem::transmute(lua::L::checkudata(l, ind, FONT_METATABLE_NAME))
    }
}

/// Checks if the value at `ind` on the stack is a [Font] and returns it if so.
///
/// This function will raise a Lua error if the value at `ind` is not a Font.
pub unsafe fn checkfont(l: &lua_State, ind: i32) -> ManuallyDrop<Arc<Font>> {
    ManuallyDrop::new(unsafe { Arc::from_raw(*checkfont_ptr(l, ind)) })
}

pub fn register_module_functions(l: &lua_State) {
    lua::pushcfunction(l, Some(get_font));
    lua::setfield(l, -2, "getfont");

    let ui = crate::overlay::ui();

    lua::newtable(l); // a table to hold default fonts: "fonts"

    crate::ui::font::lua::pushfont(l, &ui.regular_font);
    lua::setfield(l, -2, "regular");
    crate::ui::font::lua::pushfont(l, &ui.italic_font);
    lua::setfield(l, -2, "italic");
    crate::ui::font::lua::pushfont(l, &ui.mono_font);
    lua::setfield(l, -2, "monospace");
    crate::ui::font::lua::pushfont(l, &ui.icon_font);
    lua::setfield(l, -2, "icon");

    lua::setfield(l, -2, "fonts");
}

/*** RST
.. lua:function:: getfont(path, size[, vars])

    Get a font.

    If the font hasn't been loaded yet, it will be initialized first.

    :param string path: The path to the font file.
    :param integer size: The font height in pixels.
    :param table vars: (Optional) A table of font variable values, such as ``wght``.

    :rtype: uifont

    .. code-block:: lua
        :caption: Example

        local ui = require 'ui'

        local bold = ui.getfont('path/to/font.tff', 14, {wght = 900})

    .. note::

        Variables supported will differ for each font. Some fonts will not
        support variables at all.

        Unspecified, unsupported font variables or invalid values will be ignored
        and be given default values.

        Inter, the regular default font supports the following variables:

        - ``slnt``: slant
        - ``wght``: weight

        CascadeCode, the monospace default font supports the following variables:

        - ``wght``: weight

        Google Material Design Icons, the icon default font supports the following variables:

        - ``FILL``: ``0`` for outlined or ``1`` for filled icons
        - ``GRAD``: grade, similar to ``wght``
        - ``opsz``: optical size
        - ``wght``: weight

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn get_font(l: &lua_State) -> i32 {
    lua::checkargstring!(l, 1);
    lua::checkarginteger!(l, 2);

    if lua::gettop(l)>=3 && lua::luatype(l,3) != lua::LuaType::LUA_TTABLE {
        lua::pushstring(l, "get_font argument #3 must be a table of font variable values.");
        unsafe { lua::error(l); }
    }

    let path = lua::tostring(l, 1).unwrap();
    let size: u32 = lua::tointeger(l, 2) as u32;

    let mut vars: Vec<(String, i32)> = Vec::new();

    if lua::gettop(l)>=3 {
        lua::pushnil(l);

        while lua::next(l, 3) != 0 {
            let var = lua::tostring(l, -2).unwrap();
            let val = lua::tointeger(l, -1);

            vars.push((String::from(var), val as i32));

            lua::pop(l, 1);
        }
    }

    let ui = crate::overlay::ui();


    let f = ui.font_manager.get_font(&path, size, &vars);

    pushfont(l, &f);

    return 1;
}

/*** RST

Classes
-------

.. lua:class:: uifont

    A font with a unique combination of size and font variables.

    A font is used to render text within the overlay and is most commonly used
    with :lua:class:`uitext` elements.


    .. lua:method:: path()

        Return the path of the font file this font was created from.

        This exists mostly for debugging purposes.

        :rtype: string

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn path(l: &lua_State) -> i32 {
    let f = unsafe { checkfont(l, 1) };

    lua::pushstring(l, f.key.path.as_str());

    return 1;
}

unsafe extern "C" fn __gc(l: &lua_State) -> i32 {
    let font_ptr = unsafe { checkfont_ptr(l, 1) };

    let f: Arc<Font> = unsafe { Arc::from_raw(*font_ptr) };

    drop(f);

    return 0;
}

/*** RST
    .. lua:method:: tosize(newsize)

        Create a new :lua:class:`uifont` based on this font with a different size.

        :param integer newsize:

        :rtype: uifont

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn to_size(l: &lua_State) -> i32 {
    lua::checkarginteger!(l, 2);
    let f = unsafe { checkfont(l, 1) };
    let size = lua::tointeger(l, 2);

    let ui = crate::overlay::ui();

    let new_f = ui.font_manager.get_font_from_font_with_size(&f, size as u32);

    pushfont(l, &new_f);

    return 1;
}

/*** RST
    .. lua:method:: tosizeperc(sizepercent)

        Create a new :lua:class:`uifont` based on this font with a scaled size.

        This method can be used to create a font based on the EG-Overlay default
        fonts, such as a font that is 75% of the default size, etc.

        :param number sizepercent: 1.0 is the same size as this font
        :rtype: uifont

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn to_size_perc(l: &lua_State) -> i32 {
    lua::checkargnumber!(l, 2);
    let f = unsafe { checkfont(l, 1) };
    let sp = lua::tonumber(l, 2);

    let ui = crate::overlay::ui();

    let new_f = ui.font_manager.get_font_from_font_with_size_perc(&f, sp);

    pushfont(l, &new_f);

    return 1;
}
