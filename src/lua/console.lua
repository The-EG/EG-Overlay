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

local overlay_menu = require 'overlay-menu'

local win = {}
win.__index = win

function win.new()
    local w = {
        settings = overlay.settings('console.lua'),

        win = ui.window('Lua Console'),
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

        overlay_menu_mi = ui.textmenuitem('Lua Console', 0xFFFFFFFF, overlay_menu.font),
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

    w.sv:child(w.msgbox)

    setmetatable(w, win)

    w.overlay_menu_mi:addeventhandler(function() w:onmenuclick() end, 'click-left')

    w.entry:addeventhandler(function(event) w:onreturn() end, 'return-down')
    w.entry:addeventhandler(function(event) w.entrymenu.menu:show() end, 'click-right')
    w.entry:addeventhandler(function(event) w:clipboardpaste() end, 'ctrl-v-down')
    w.entrymenu.paste:addeventhandler(function(event) w:clipboardpaste() end, 'click-left')

    if w.settings:get('window.visible') then
        w.win:show()
        w.overlay_menu_mi:icon(overlay_menu.visible_icon)
    else
        w.overlay_menu_mi:icon(overlay_menu.hidden_icon)
    end

    return w
end

function win:onmenuclick()
    if self.settings:get('window.visible') then
        self.win:hide()
        self.settings:set('window.visible', false)
        self.overlay_menu_mi:icon(overlay_menu.hidden_icon)
    else
        self.win:show()
        self.settings:set('window.visible', true)
        self.overlay_menu_mi:icon(overlay_menu.visible_icon)
    end
end

function win:clipboardpaste()
    local t = overlay.clipboardtext()
    self.entry:text(t)
    self.entrymenu.menu:hide()
end

function win:show()
    self.win:show()
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

-- redefine print so that it prints on the console. kind of hacky...but eh?
function print(...)
    local strs = {}

    for i,p in ipairs({...}) do
        table.insert(strs, tostring(p))
    end
    console:addmessage(table.concat(strs, ' '), 0xFFFFFFFF)
end


overlay.addeventhandler('log-message', function(event, message) console:onlogmessage(message) end)
overlay.addeventhandler('startup', function()
    overlay_menu.additem(console.overlay_menu_mi)
end)
return {}
