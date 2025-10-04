-- EG-Overlay
-- Copyright (c) 2025 Taylor Talkington
-- SPDX-License-Identifier: MIT

--[[ RST
Overlay Menu
============


]]--
local overlay = require 'overlay'
local ui = require 'ui'

local M = {}

local menu = ui.menu()
local itemsbefore = ui.separatormenuitem('horizontal')

local function setupmenu()
    local c = ui.color('accentText')
    local ver = overlay.versionstring()

    local title = ui.textmenuitem("EG-Overlay " .. ver, c, ui.fonts.regular)
    title:enabled(false)

    local settings = ui.textmenuitem('Settings', ui.color('text'), ui.fonts.regular)
    local restart = ui.textmenuitem('Restart', ui.color('text'), ui.fonts.regular)

    menu:pushback(title)
    menu:pushback(ui.separatormenuitem('horizontal'))
    menu:pushback(itemsbefore)
    menu:pushback(settings)
    menu:pushback(restart)

    settings:addeventhandler(function()
        overlay.logerror("Overlay settings not implemented yet.")
        menu:hide()
    end, 'click-left')

    restart:addeventhandler(function()
        overlay.restart()
    end, 'click-left')
end

function M.additem(label, icon, actionhandler)
    local mi = ui.textmenuitem(label, ui.color('text'), ui.fonts.regular)

    if icon then
        mi:icon(ui.iconcodepoint(icon))
    end

    if actionhandler then
        mi:addeventhandler(actionhandler, 'click-left')
    end

    menu:insertbefore(itemsbefore, mi)
end

setupmenu()

overlay.addkeybindhandler('ctrl-shift-e', function()
    menu:show()

    return true
end)

return M
