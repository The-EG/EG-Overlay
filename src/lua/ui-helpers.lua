local overlay = require 'eg-overlay'
local ui = require 'eg-overlay-ui'

local uih = {}

function uih.text(text, color)
    local settings = overlay.settings()
    local font_name = settings:get('overlay.ui.font.path')
    local font_size = settings:get('overlay.ui.font.size')
    local color = color or settings:get('overlay.ui.colors.text')

    return ui.text(text, color, font_name, font_size)
end

function uih.monospace_text(text, color)
    local settings = overlay.settings()
    local font_name = settings:get('overlay.ui.font.pathMono')
    local font_size = settings:get('overlay.ui.font.size')
    local color = color or settings:get('overlay.ui.colors.text')

    return ui.text(text, color, font_name, font_size)
end

function uih.monospace_text_menu_item(text)
    local t = uih.monospace_text(text)
    local mi = ui.menu_item()
    mi:set_child(t)

    return mi
end

function uih.text_menu_item(text)
    local t = uih.text(text)
    local mi = ui.menu_item()
    mi:set_child(t)

    return mi
end

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

function uih.checkbox()
    local settings = overlay.settings()
    local font_size = settings:get('overlay.ui.font.size')

    return ui.checkbox(font_size * 1.25)
end

return uih