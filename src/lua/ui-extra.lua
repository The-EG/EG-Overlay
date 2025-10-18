-- EG-Overlay
-- Copyright (c) 2025 Taylor Talkington
-- SPDX-License-Identifier: MIT

local ui = require 'ui'

local ERROR_COLOR = 0xeb4034ff

local M = {}

local RGBEntryMetaTable = {}

function RGBEntryMetaTable:__index(key)
    if RGBEntryMetaTable[key] then return RGBEntryMetaTable[key] end

    return RGBEntryMetaTable.__super_metatable[key]
end

function RGBEntryMetaTable:__gc()
    RGBEntryMetaTable.__super_metatable.__gc(self)
end

function RGBEntryMetaTable:validate()
    local color = self:colorvalue()
    if color == nil then
        self:bgcolor(ERROR_COLOR)
        return
    else
        self:bgcolor(ui.color('entryBG'))
    end

    self:sendevent('color-updated')
end

function RGBEntryMetaTable:colorvalue(value)
    if value then
        self:text(string.format('#%06X', value))
        return
    end

    local val = self:text()

    local r,g,b = string.match(val, '^#(%x%x)(%x%x)(%x%x)$')

    if r == nil then return end

    local red = tonumber(r, 16)
    local green = tonumber(g, 16)
    local blue = tonumber(b, 16)

    local color = (red << 16) | (green << 8) | blue

    return color
end

function M.rgbentry(font)
    local e = ui.entry(font)

    if not RGBEntryMetaTable.__super_metatable then
        local orig_mt = debug.getmetatable(e)
        RGBEntryMetaTable.__super_metatable = orig_mt
    end

    debug.setmetatable(e, RGBEntryMetaTable)

    e:addeventhandler(function() e:validate() end, 'unfocus', 'return-down', 'tab-down')

    return e
end

local FloatEntryMetaTable = {}

function FloatEntryMetaTable:__index(key)
    if FloatEntryMetaTable[key] then return FloatEntryMetaTable[key] end

    return FloatEntryMetaTable.__super_metatable[key]
end

function FloatEntryMetaTable:__gc()
    FloatEntryMetaTable.__super_metatable.__gc(self)
end

function FloatEntryMetaTable:floatvalue(value, precision)
    if value then
        precision = precision or 3
        local fmt = string.format('%%0.0%df', precision)
        self:text(string.format(fmt, value))
        return
    end

    local val = self:text()

    return tonumber(self:text())
end

function FloatEntryMetaTable:validate()
    local val = self:floatvalue()

    if val == nil then
        self:bgcolor(ERROR_COLOR)
        return
    else
        self:bgcolor(ui.color('entryBG'))
    end

    self:sendevent('float-updated')
end

function M.floatentry(font)
    local e = ui.entry(font)

    if not FloatEntryMetaTable.__super_metatable then
        local orig_mt = debug.getmetatable(e)
        FloatEntryMetaTable.__super_metatable = orig_mt
    end

    debug.setmetatable(e, FloatEntryMetaTable)

    e:addeventhandler(function() e:validate() end, 'unfocus', 'return-down', 'tab-down')

    return e
end

local IntEntryMetaTable = {}

function IntEntryMetaTable:__index(key)
    if IntEntryMetaTable[key] then return IntEntryMetaTable[key] end

    return IntEntryMetaTable.__super_metatable[key]
end

function IntEntryMetaTable:__gc()
    IntEntryMetaTable.__super_metatable.__gc(self)
end

function IntEntryMetaTable:intvalue(value)
    if value then
        self:text(string.format('%d', value))
        return
    end

    local val = self:text()

    return tonumber(self:text(), 10)
end

function IntEntryMetaTable:validate()
    local val = self:intvalue()

    if val == nil then
        self:bgcolor(ERROR_COLOR)
        return
    else
        self:bgcolor(ui.color('entryBG'))
    end

    self:sendevent('int-updated')
end

function M.intentry(font)
    local e = ui.entry(font)

    if not IntEntryMetaTable.__super_metatable then
        local orig_mt = debug.getmetatable(e)
        IntEntryMetaTable.__super_metatable = orig_mt
    end

    debug.setmetatable(e, IntEntryMetaTable)

    e:addeventhandler(function() e:validate() end, 'unfocus', 'return-down', 'tab-down')

    return e
end

local ToggleMenuItem = {}

function ToggleMenuItem:__index(key)
    if ToggleMenuItem[key] then return ToggleMenuItem[key] end

    return ToggleMenuItem.__super_metatable[key]
end

function ToggleMenuItem:__gc()
    ToggleMenuItem.__super_metatable.__gc(self)
end

function ToggleMenuItem:state(value)
    if value then
        self:icon(ui.iconcodepoint('check'))
    else
        self:icon('')
    end
end

function M.togglemenuitem(text, color, font)
    local mi = ui.textmenuitem(text, color, font)

    if not ToggleMenuItem.__super_metatable then
        local orig_mt = debug.getmetatable(mi)
        ToggleMenuItem.__super_metatable = orig_mt
    end

    debug.setmetatable(mi, ToggleMenuItem)

    mi:addeventhandler(function()
        if not mi:icon() then
            mi:icon(ui.iconcodepoint('check'))
            mi:sendevent('toggle-on')
        else
            mi:icon('')
            mi:sendevent('toggle-off')
        end
    end, 'click-left')

    return mi
end

return M
