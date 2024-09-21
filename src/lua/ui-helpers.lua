--[[ RST
ui-helpers
==========

.. lua:module:: ui-helpers

.. code-block:: lua

    local uih = require 'ui-helpers'

UI helper utility functions.

Functions
---------
]]--

local overlay = require 'eg-overlay'
local ui = require 'eg-overlay-ui'

local uih = {}

--[[ RST
.. lua:function:: text(text[, color])

    Create a :lua:class:`eg-overlay-ui.uitext` using default font settings. If
    ``color`` is not specified, the default color will also be used.

    If ``color`` is instead ``true`` then the default accent color will be used.

    :param string text:
    :param integer color: (Optional) See :ref:`colors`.
    :rtype: eg-overlay-ui.uitext

    .. versionhistory::
        :0.0.1: Added
        :0.1.0: Use accent color if ``color`` is ``true``
]]--
function uih.text(text, color)
    local settings = overlay.settings()
    local font_name = settings:get('overlay.ui.font.path')
    local font_size = settings:get('overlay.ui.font.size')
    local color = color or settings:get('overlay.ui.colors.text')

    if (type(color)=='boolean' and color) then
        color = settings:get('overlay.ui.colors.accentText')
    end

    return ui.text(text, color, font_name, font_size)
end

--[[ RST
.. lua:function:: monospace_text(text[, color])

    Create a :lua:class:`eg-overlay-ui.uitext` using default font settings for a
    monospace font. If ``color`` is not specified, the default color will be
    used.

    :param string text:
    :param integer color: (Optional) See :ref:`colors`.
    :rtype: eg-overlay-ui.uitext

    .. versionhistory::
        :0.0.1: Added
]]--
function uih.monospace_text(text, color)
    local settings = overlay.settings()
    local font_name = settings:get('overlay.ui.font.pathMono')
    local font_size = settings:get('overlay.ui.font.size')
    local color = color or settings:get('overlay.ui.colors.text')

    return ui.text(text, color, font_name, font_size)
end

--[[ RST
.. lua:function:: monospace_text_menu_item(text)

    Create a :lua:class:`eg-overlay-ui.uimenuitem` containing a
    :lua:class:`eg-overlay-ui.text` using the default font settings for a
    monospace font.

    :param string text:
    :rtype: eg-overlay-ui.uimenuitem

    .. versionhistory::
        :0.0.1: Added
]]--
function uih.monospace_text_menu_item(text)
    local t = uih.monospace_text(text)
    local mi = ui.menu_item()
    mi:set_child(t)

    return mi
end

--[[ RST
.. lua:function:: text_menu_item(text)

    Create a :lua:class:`eg-overlay-ui.uimenuitem` containing a
    :lua:class:`eg-overlay-ui.text` using the default font settings.

    :param string text:
    :rtype: eg-overlay-ui.uimenuitem

    .. versionhistory::
        :0.0.1: Added
]]--
function uih.text_menu_item(text)
    local t = uih.text(text)
    local mi = ui.menu_item()
    mi:set_child(t)

    return mi
end

--[[ RST
.. lua:function:: text_button(text)

    Create a :lua:class:`eg-overlay-ui.uibutton` containing a
    :lua:class:`eg-overlay-ui.uitext` using the default font settings.

    :param string text:
    :rtype: eg-overlay-ui.uibutton

    .. versionhistory::
        :0.0.1: Added
]]--
function uih.text_button(text)
    local t = uih.text(text)
    local box = ui.box('horizontal')
    box:padding(5,5,2,2)
    local btn = ui.button()
    btn:set_child(box)
    box:align('middle')
    box:pack_end(t, false, 'start')

    return btn
end

--[[ RST
.. lua:function:: checkbox()

    Create a :lua:class:`eg-overlay-ui.uibutton` that is specialized as a
    checkbox and automatically sized based on the default font settings.

    :rtype: eg-overlay-ui.uibutton

    .. versionhistory::
        :0.0.1: Added
]]--
function uih.checkbox()
    local settings = overlay.settings()
    local font_size = settings:get('overlay.ui.font.size')

    return ui.checkbox(font_size * 1.25)
end

return uih
