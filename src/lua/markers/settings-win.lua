-- EG-Overlay
-- Copyright (c) 2025 Taylor Talkington
-- SPDX-License-Identifier: MIT

local settings = require 'markers.settings'
local ui = require 'ui'
local uiextra = require 'ui-extra'

local M = {}

settings:setdefault('settingsWindow.x', 300)
settings:setdefault('settingsWindow.y', 100)
settings:setdefault('settingsWindow.width', 100)
settings:setdefault('settingsWindow.height', 100)
settings:setdefault('settingsWindow.visible', false)

local function textbutton(text)
    local btn = ui.button()
    local box = ui.box('vertical')
    local lbl = ui.text(text, ui.color('text'), ui.fonts.regular)

    box:paddingleft(10)
    box:paddingright(10)
    box:paddingtop(5)
    box:paddingbottom(5)
    box:pushback(lbl, 'middle', false)
    btn:child(box)

    return btn
end

local function iconbutton(icon, color)
    local btn = ui.button()
    local box = ui.box('vertical')
    local lbl = ui.text(ui.iconcodepoint(icon), color, ui.fonts.icon)

    box:paddingleft(2)
    box:paddingright(2)
    box:paddingtop(2)
    box:paddingbottom(2)
    box:pushback(lbl, 'middle', false)
    btn:child(box)

    return btn
end

local ColorSetting = {}
ColorSetting.__index = ColorSetting

function ColorSetting.new(label, key)
    local c = {}

    c.key = key
    c.lbl = ui.text(label, ui.color('text'), ui.fonts.regular)
    c.txt = uiextra.rgbentry(ui.fonts.monospace)
    c.txt:prefwidth(75)
    c.txt:colorvalue(settings:get(key))
    c.resetbtn = iconbutton('reset_settings', ui.color('text'))

    setmetatable(c, ColorSetting)
    return c
end

function ColorSetting:addupdateeventhandler(fnc)
    self.txt:addeventhandler(fnc, 'color-updated')
end

function ColorSetting:updatesetting()
    local val = self.txt:colorvalue()

    if val then
        settings:set(self.key, val)
        self.txt:colorvalue(settings:get(self.key))
    end
end

function ColorSetting:clearsetting()
    settings:remove(self.key)

    self.txt:colorvalue(settings:get(self.key))
end

local FloatSetting = {}
FloatSetting.__index = FloatSetting

function FloatSetting.new(label, key)
    local c = {}

    c.key = key
    c.lbl = ui.text(label, ui.color('text'), ui.fonts.regular)
    c.txt = uiextra.floatentry(ui.fonts.monospace)
    c.txt:prefwidth(75)
    c.txt:floatvalue(settings:get(key))
    c.resetbtn = iconbutton('reset_settings', ui.color('text'))

    setmetatable(c, FloatSetting)
    return c
end

function FloatSetting:addupdateeventhandler(fnc)
    self.txt:addeventhandler(fnc, 'float-updated')
end

function FloatSetting:updatesetting()
    local val = self.txt:floatvalue()

    if val then
        settings:set(self.key, val)
        self.txt:floatvalue(settings:get(self.key))
    end
end

function FloatSetting:clearsetting()
    settings:remove(self.key)

    self.txt:floatvalue(settings:get(self.key))
end

local IntSetting = {}
IntSetting.__index = IntSetting

function IntSetting:addupdateeventhandler(fnc)
    self.txt:addeventhandler(fnc, 'int-updated')
end

function IntSetting.new(label, key)
    local c = {}

    c.key = key
    c.lbl = ui.text(label, ui.color('text'), ui.fonts.regular)
    c.txt = uiextra.intentry(ui.fonts.monospace)
    c.txt:prefwidth(75)
    c.txt:intvalue(settings:get(key))
    c.resetbtn = iconbutton('reset_settings', ui.color('text'))

    setmetatable(c, IntSetting)
    return c
end

function IntSetting:updatesetting()
    local val = self.txt:intvalue()

    if val then
        settings:set(self.key, val)
        self.txt:intvalue(settings:get(self.key))
    end
end

function IntSetting:clearsetting()
    settings:remove(self.key)

    self.txt:intvalue(settings:get(self.key))
end

local SettingsWindow = {}
SettingsWindow.__index = SettingsWindow

function SettingsWindow.new()
    local w = {}

    w.win = ui.window('Markers Settings')
    w.win:settings(settings, 'settingsWindow')

    w.outerbox = ui.box('vertical')
    w.outerbox:paddingleft(5)
    w.outerbox:paddingright(5)
    w.outerbox:paddingtop(5)
    w.outerbox:paddingbottom(5)
    w.outerbox:spacing(2)

    w.win:child(w.outerbox)

    w.packslbl = {
        box = ui.box('horizontal'),
        lbl = ui.text('Marker packs:', ui.color('text'), ui.fonts.regular),
        btnbox = ui.box('vertical'),
        btn = iconbutton('add', 0x259C2BFF),
    }

    w.outerbox:pushback(w.packslbl.box, 'fill', false)
    w.packslbl.box:spacing(10)
    w.packslbl.box:pushback(w.packslbl.lbl, 'middle', false)
    w.packslbl.box:pushback(w.packslbl.btnbox, 'middle', true)
    w.packslbl.btnbox:pushback(w.packslbl.btn, 'end', false)

    w.outerbox:pushback(ui.separator('horizontal'), 'fill', false)

    w.settingsgrid = ui.grid(6, 3)
    w.settingsgrid:colspacing(10)
    w.settingsgrid:rowspacing(2)
    w.outerbox:pushback(w.settingsgrid, 'fill', false)

    w.settings = {
        ColorSetting.new('Default Marker Color', 'defaultMarkerColor'),
        FloatSetting.new('Default Marker Alpha', 'defaultMarkerAlpha'),
        FloatSetting.new('Default Marker Height Offset', 'defaultHeightOffset'),
        FloatSetting.new('Default Marker Size Ratio', 'defaultIconSize'),
        IntSetting.new('Marker Base Size', 'markerBaseSize'),
        IntSetting.new('Default Marker Map Size', 'defaultMarkerMapDisplaySize'),
    }

    for row, setting in ipairs(w.settings) do
        setting:addupdateeventhandler(function() setting:updatesetting() end)
        setting.resetbtn:addeventhandler(function() setting:clearsetting() end, 'click-left')

        w.settingsgrid:attach(setting.lbl, row, 1, 1, 1, 'start', 'middle')
        w.settingsgrid:attach(setting.txt, row, 2, 1, 1, 'fill', 'middle')
        w.settingsgrid:attach(setting.resetbtn, row, 3, 1, 1, 'middle', 'middle')
    end

    local warntxt = 'Changes to the above settings will take affect the next\n' ..
                    'time markers are selected/loaded.'
    w.warnlbl = ui.text(warntxt, 0xD6C796FF, ui.fonts.italic)
    w.outerbox:pushback(w.warnlbl, 'start', false)

    w.outerbox:pushback(ui.separator('horizontal'), 'fill', false)

    w.closebtn = textbutton('Close')

    w.outerbox:pushback(w.closebtn, 'end', false)

    w.closebtn:addeventhandler(function()
        w:hide()
    end, 'click-left')

    setmetatable(w, SettingsWindow)
    return w
end

function SettingsWindow:show()
    if self.packsgrid then
        self.outerbox:removeitem(self.packsgrid)
        self.packsgrid = nil
    end

    settings:set('settingsWindow.visible', true)

    self.win:show()

    local packpaths = settings:get('markerpacks')

    if not packpaths or #packpaths == 0 then return end

    self.packsgrid = ui.grid(#packpaths, 2)
    self.packsgrid:colspacing(10)
    self.outerbox:insertafter(self.packslbl.box, self.packsgrid, 'fill', false)

    for i,path in ipairs(packpaths) do
        local lbl = ui.text(path, ui.color('text'), ui.fonts.regular)
        local btn = iconbutton('remove', 0xC22929FF)

        self.packsgrid:attach(lbl, i, 1, 1, 1, 'start', 'middle')
        self.packsgrid:attach(btn, i, 2, 1, 1, 'middle', 'middle')
    end
end

function SettingsWindow:hide()
    self.win:hide()

    settings:set('settingsWindow.visible', false)
end

M.win = SettingsWindow.new()

function M.show()
    M.win:show()
end

function M.hide()
    M.win:hide()
end

if settings:get('settingsWindow.visible') then
    M.show()
end

return M
