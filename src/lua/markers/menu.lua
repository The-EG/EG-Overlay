-- EG-Overlay
-- Copyright (c) 2025 Taylor Talkington
-- SPDX-License-Identifier: MIT
local overlay  = require 'eg-overlay'
local ui       = require 'eg-overlay-ui'
local uih      = require 'ui-helpers'
local settings = require 'markers.settings'

local function checkboxmenuitem(text)
    local mi = ui.menu_item()
    local osettings = overlay.settings()
    local font_name = osettings:get('overlay.ui.font.path')
    local font_size = osettings:get('overlay.ui.font.size')
    local mit = ui.text(text, 0xFFFFFFFF, font_name, font_size)
    local mic = uih.checkbox()

    mi:set_child(mit)
    mi:set_pre(mic)
    return {menuitem = mi, checkbox = mic, text = mit}
end

local function menuitem(text)
    local mi = ui.menu_item()
    local osettings = overlay.settings()
    local font_name = osettings:get('overlay.ui.font.path')
    local font_size = osettings:get('overlay.ui.font.size')
    local mit = ui.text(text, 0xFFFFFFFF, font_name, font_size)
    mi:set_child(mit)
    return { menuitem = mi, text = mit}
end

local function sepmenuitem()
    local sep = ui.separator('horizontal')
    local mi = ui.menu_item()
    mi:set_child(sep)

    return {menuitem = mi, separator = sep}
end

local mainmenu = {
    menu            = ui.menu(),
    showmarkers     = checkboxmenuitem('Show Markers'),
    showtooltips    = checkboxmenuitem('Show Tooltips'),
    categorymanager = menuitem('Manage Markers'),
    sep1            = sepmenuitem(),
    settings        = menuitem('Settings'),
}

local function drawmarkerstoggled(event)
    if event~='click-left' then return end

    settings:set('drawMarkers', not settings:get('drawMarkers'))
    mainmenu.showmarkers.checkbox:state(settings:get('drawMarkers'))
end

local function showtooltipstoggled(event)
    if event~='click-left' then return end

    settings:set('showTooltips', not settings:get('showTooltips'))
    mainmenu.showtooltips.checkbox:state(settings:get('showTooltips'))
end

mainmenu.showmarkers.checkbox:state(settings:get('drawMarkers'))
mainmenu.showmarkers.checkbox:addeventhandler(drawmarkerstoggled)
mainmenu.showmarkers.menuitem:addeventhandler(drawmarkerstoggled)

mainmenu.showtooltips.checkbox:state(settings:get('showTooltips'))
mainmenu.showtooltips.checkbox:addeventhandler(showtooltipstoggled)
mainmenu.showtooltips.menuitem:addeventhandler(showtooltipstoggled)

mainmenu.categorymanager.menuitem:addeventhandler(function(event)
    if event=='click-left' then
        require('markers.category-manager').show()
        mainmenu.menu:hide()
    end
end)

mainmenu.sep1.menuitem:enabled(false)

mainmenu.menu:add_item(mainmenu.showmarkers.menuitem)
mainmenu.menu:add_item(mainmenu.showtooltips.menuitem)
mainmenu.menu:add_item(mainmenu.categorymanager.menuitem)
mainmenu.menu:add_item(mainmenu.sep1.menuitem)
mainmenu.menu:add_item(mainmenu.settings.menuitem)

local function onmainmenuevent(event)
    if event=='click-left' then
        local x,y = ui.mouseposition()
        mainmenu.menu:show(x,y)
    end
end

local function onstartup()
    overlay.queueevent('register-module-actions', {
        name = "Markers",
        primary_action = onmainmenuevent
    })
end

overlay.addeventhandler('startup', onstartup)

return {}
