-- EG-Overlay
-- Copyright (c) 2025 Taylor Talkington
-- SPDX-License-Identifier: MIT

--[[ RST
Overlay Menu
============


]]--
local overlay = require 'eg-overlay'
local ui = require 'eg-overlay-ui'

local M = {}

M.menu = ui.menu()
M.itemsbefore = ui.separatormenuitem('horizontal')

local function setupmenu()
    local c = ui.color('accentText')
    local ver = overlay.versionstring()

    local title = ui.textmenuitem("EG-Overlay " .. ver, c, ui.fonts.regular)
    title:enabled(false)

    local settings = ui.textmenuitem('Settings', ui.color('text'), ui.fonts.regular)
    local restart = ui.textmenuitem('Restart', ui.color('text'), ui.fonts.regular)

    M.menu:pushback(title)
    M.menu:pushback(ui.separatormenuitem('horizontal'))
    M.menu:pushback(M.itemsbefore)
    M.menu:pushback(settings)
    M.menu:pushback(restart)

    settings:addeventhandler(function()
        overlay.logerror("Overlay settings not implemented yet.")
        M.menu:hide()
    end, 'click-left')

    restart:addeventhandler(function()
        overlay.restart()
    end, 'click-left')
end

local function onhotkey()
    M.show()

    return true
end

function M.show()
    M.menu:show()
end

function M.hide()
    M.menu:hide()
end

function M.additem(menuitem)
    --M.menu:pushback(menuitem)
    M.menu:insertbefore(M.itemsbefore, menuitem)
end

M.font = ui.fonts.regular
M.visible_icon = ui.iconcodepoint('visibility')
M.hidden_icon = ui.iconcodepoint('visibility_off')

setupmenu()

overlay.addkeybindhandler('ctrl-shift-e', onhotkey)

return M
