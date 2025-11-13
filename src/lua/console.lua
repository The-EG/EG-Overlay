-- EG-Overlay
-- Copyright (c) 2025 Taylor Talkington
-- SPDX-License-Identifier: MIT

--[[ RST
Lua Console/Log
===============

.. overlay:module:: console

The Lua Console can be used to run Lua commands interactively and also displays
log messages.

Lua commands are run as coroutines, long running commands that yield properly
will not cause the UI to freeze.

Settings
--------

The following settings for the Lua Console are stored in ``settings/console.lua.json``.

.. overlay:modsetting:: window.x
    :type: integer
    :default: 50

    The window X position.

    .. versionhistory::
        :0.3.0: Added

.. overlay:modsetting:: window.y
    :type: integer
    :default: 50

    The window Y position.

    .. versionhistory::
        :0.3.0: Added

.. overlay:modsetting:: window.width
    :type: integer
    :default: 400

    The window width.

    .. versionhistory::
        :0.3.0: Added

.. overlay:modsetting:: window.height
    :type: integer
    :default: 200

    The window height.

    .. versionhistory::
        :0.3.0: Added

.. overlay:modsetting:: window.visible
    :type: boolean
    :default: false

    If the window is displayed.

    .. versionhistory::
        :0.3.0: Added

.. overlay:modsetting:: colors.ERROR
    :type: integer
    :default: 0x911717FF

    The color used to display ERROR messages.

    .. versionhistory::
        :0.3.0: Added

.. overlay:modsetting:: colors.WARNING
    :type: integer
    :default: 0xb8326FF

    The color used to display WARNING messages.

    .. versionhistory::
        :0.3.0: Added

.. overlay:modsetting:: colors.DEBUG
    :type: integer
    :default: 0x676F80FF

    The color used to display DEBUG messages.

    .. versionhistory::
        :0.3.0: Added

.. overlay:modsetting:: colors.INFO
    :type: integer
    :default: 0xFFFFFFFF

    The color used to display INFO messages.

    .. versionhistory::
        :0.3.0: Added
]]--
local ui = require 'ui'
local overlay = require 'overlay'
local dialogs = require 'dialogs'

local overlay_menu = require 'overlay-menu'

local function iconbutton(icon)
    local btn = ui.button()
    local icon = ui.text(ui.iconcodepoint(icon), ui.color('text'), ui.fonts.icon)

    btn:child(icon)
    btn:bgcolor(0x00000000)
    btn:borderwidth(0)

    return btn
end

local win = {}
win.__index = win

function win.new()
    local w = {
        settings = overlay.settings('console.lua'),

        win = ui.window('Lua Console'),
        close_btn = iconbutton('close'),
        about_btn = iconbutton('info_i'),
        sv = ui.scrollview(),
        outerbox = ui.box('vertical'),
        msgbox = ui.box('vertical'),

        prompt_box = ui.box('horizontal'),
        prompt_lbl = ui.text('Lua>', 0xFFFFFFFF, ui.fonts.monospace),

        entry = ui.entry(ui.fonts.monospace),

        entrymenu = {
            menu = ui.menu(),
            paste = ui.textmenuitem('Paste', 0xFFFFFFFF, ui.fonts.regular),
        },
    }

    w.settings:setdefault('window.x', 50)
    w.settings:setdefault('window.y', 50)
    w.settings:setdefault('window.width', 400)
    w.settings:setdefault('window.height', 200)
    w.settings:setdefault('window.visible', false)

    w.settings:setdefault('colors.ERROR'  , 0x911717FF)
    w.settings:setdefault('colors.WARNING', 0xb58326FF)
    w.settings:setdefault('colors.DEBUG'  , 0x676F80FF)
    w.settings:setdefault('colors.INFO'   , 0xFFFFFFFF)

    w.colors = {
        ERROR   = w.settings:get('colors.ERROR'),
        WARNING = w.settings:get('colors.WARNING'),
        DEBUG   = w.settings:get('colors.DEBUG'),
        INFO    = w.settings:get('colors.INFO'),
    }

    w.msgbox:spacing(1)
    w.msgbox:paddingbottom(1)

    w.outerbox:paddingleft(5)
    w.outerbox:paddingright(5)
    w.outerbox:paddingtop(5)
    w.outerbox:paddingbottom(5)
    w.outerbox:spacing(5)

    w.outerbox:pushback(w.sv, 'fill', true)

    w.outerbox:pushback(w.prompt_box, 'fill', false)

    w.entry:hint('(Enter Lua command)')

    w.entrymenu.menu:pushback(w.entrymenu.paste)

    w.prompt_box:spacing(5)
    w.prompt_box:pushback(w.prompt_lbl, 'middle', false)
    w.prompt_box:pushback(w.entry, 'start', true)

    w.win:child(w.outerbox)
    w.win:resizable(true)
    w.win:settings(w.settings, 'window')

    w.win:titlebarbox():pushback(w.about_btn, 'middle', false)
    w.win:titlebarbox():pushback(w.close_btn, 'middle', false)

    w.sv:child(w.msgbox)

    setmetatable(w, win)

    w.entry:addeventhandler(function(event) w:onreturn() end, 'return-down')
    w.entry:addeventhandler(function(event) w.entrymenu.menu:show() end, 'click-right')
    w.entry:addeventhandler(function(event) w:clipboardpaste() end, 'ctrl-v-down')
    w.entrymenu.paste:addeventhandler(function(event) w:clipboardpaste() end, 'click-left')

    if w.settings:get('window.visible') then
        w.win:show()
    end

    w.about_btn:addeventhandler(function() w:showabout() end, 'click-left')
    w.close_btn:addeventhandler(function(event) w:hide() end, 'click-left')

    return w
end

function win:show()
    self.win:show()
    self.settings:set('window.visible', true)
end

function win:hide()
    self.win:hide()
    self.settings:set('window.visible', false)
end

function win:showabout()
    local aboutmsg = table.concat({
        "Lua Console",
        "\u{A9} 2025 Taylor Talkington (TheEG.5873)",
        "",
        "This module is bundled with EG-Overlay,",
        "see the EG-Overlay documentation for more information."
    }, "\n")

    local d = dialogs.MessageDialog.new("About Lua Console", aboutmsg, 'info')
    d:show()
end

function win:clipboardpaste()
    local t = overlay.clipboardtext()
    self.entry:text(t)
    self.entrymenu.menu:hide()
end

function win:addmessage(msg, color)
    local txt = ui.text(msg, color, ui.fonts.monospace)
    self.msgbox:pushback(txt, 'start', false)

    if #self.msgbox > 1000 then
        self.msgbox:popfront()
    end

    self.sv:scrolly(1.0)
end

function win:onlogmessage(message)
    local ts, lvl, tgt, msg = message:match('(%d+%-%d+%-%d+ %d+:%d+:%d+%.%d+) | +([%a]+) +| ([^|]+) | (.*)')

    self:addmessage(message, self.colors[lvl])
end

function win:onreturn()
    local cmd = self.entry:text()

    if cmd == nil or cmd == '' then return end

    self:addmessage('Lua> ' .. cmd, 0xFFFFFFFF)

    self.entry:text('')

    local func, load_err = load(cmd, 'Lua Console', 't')

    if not func then
        self:addmessage(load_err, self.colors.ERROR)
        return
    end

    -- The code that the user submits may result in a coroutine being run.
    -- If that coroutine causes an error it won't be displayed through the log,
    -- so manually run a coroutine here (becoming a coroutine ourselves) and log
    -- the error if it occurs.
    local func_thread = coroutine.create(func)

    while coroutine.status(func_thread)~='dead' do
        local ok, err = coroutine.resume(func_thread)

        if not ok then
            coroutine.close(func_thread)
            overlay.logerror(string.format('Error while running console input: %s', err))

            return
        end
        coroutine.yield()
    end
    coroutine.close(func_thread)
end

local console = win.new()

overlay.addeventhandler('log-message', function(event, message) console:onlogmessage(message) end)

overlay_menu.additem('Lua Console', 'terminal', function()
    if console.settings:get('window.visible') then
        console:hide()
    else
        console:show()
    end
end)

return {}
