-- EG-Overlay
-- Copyright (c) 2025 Taylor Talkington
-- SPDX-License-Identifier: MIT

--[[ RST
Markers Category Manager
========================

This window allows users to control what markers from the loaded marker packs
are displayed in the overlay.
]]--

local overlay = require 'overlay'
local ui = require 'ui'
local settings = require 'markers.settings'
local manager = require 'markers.manager'
local ml = require 'mumble-link'
local data = require 'markers.data'
local overlay_menu = require 'overlay-menu'

local M = {}

local overlay_settings = overlay.overlaysettings()
local monofontsize = overlay_settings:get('overlay.ui.font.mono.size')

local function checkbox()
    local c = ui.checkbox(monofontsize + 4)
    c:bgcolor(0x00000000)
    c:bghover(0x757575FF)
    c:border(0xFFFFFFFF)
    c:borderwidth(1)

    return c
end

local childrenicon = ui.iconcodepoint('subdirectory_arrow_right')

local function childrenbutton()
    local btn = ui.button()

    local txt = ui.text(childrenicon, ui.color('text'), ui.fonts.icon:tosizeperc(0.85))

    local box = ui.box('horizontal')
    box:paddingleft(5)
    box:paddingright(5)
    box:alignment('middle')

    box:pushback(txt, 'start', false)

    btn:child(box)

    return btn
end

local function textbutton(text)
    --local osettings = overlay.overlaysettings()
    local txt = ui.text(text, ui.color('text'), ui.fonts.regular)

    local box = ui.box('vertical')
    box:paddingleft(5)
    box:paddingright(5)
    box:paddingtop(5)
    box:paddingbottom(5)
    box:pushback(txt, 'middle', false)

    local btn = ui.button()
    btn:child(box)

    return btn
end

local function shortbutton(text)
    local txt = ui.text(text, ui.color('text'), ui.fonts.regular)

    local box = ui.box('vertical')
    box:paddingleft(5)
    box:paddingright(5)
    box:pushback(txt, 'middle', false)

    local btn = ui.button()
    btn:child(box)

    return btn
end


-- The path of the categories displayed. This is a category typeid, of which
-- all child categories are displayed.
settings:setdefault('categoryManager.path', '')
settings:setdefault('categoryManager.onlyShowCategoriesInMap', false)

settings:setdefault('categoryManager.window.x', 200)
settings:setdefault('categoryManager.window.y', 50)
settings:setdefault('categoryManager.window.width', 300)
settings:setdefault('categoryManager.window.height', 600)
settings:setdefault('categoryManager.window.visible', false)

local BreadCrumbBox = {}
BreadCrumbBox.__index = BreadCrumbBox

function BreadCrumbBox.new()
    local b = {}
    b.box = ui.box('vertical')

    b.home =  {
        box = ui.box('horizontal'),
        check = checkbox(),
        btn = shortbutton('All Markers'),
        path = '',
    }

    b.handlers = {}

    setmetatable(b, BreadCrumbBox)

    --b.home.box:paddingleft(3)
    b.home.box:spacing(5)
    b.home.box:pushback(b.home.check, 'middle', false)
    b.home.box:pushback(b.home.btn  , 'middle', false)

    b.home.check:checkstate(settings:get('showMarkers'))

    b.home.check:addeventhandler(function(e)
        manager.setshowmarkers(e=='toggle-on')
    end, 'toggle-on', 'toggle-off')

    b.home.btn:addeventhandler(function()
        b:_runhandlers('')
    end, 'click-left')



    b.box:spacing(2)
    b.box:pushback(b.home.box, 'start', false)

    b:update()

    return b
end

function BreadCrumbBox:update()
    while #self.box > 1 do self.box:popback() end

    local path = settings:get('categoryManager.path')
    if path == '' then return end

    for name, mp in pairs(manager.markerpacks) do
        local category = mp:category(path)

        if category then
            local pathcats = {}

            local c = category
            while c do
                table.insert(pathcats, 1, c)
                c = c:parent()
            end

            for i, c in ipairs(pathcats) do
                local box = ui.box('horizontal')
                local check = checkbox()
                local btn = shortbutton(c.displayname)

                check:checkstate(data.iscategoryactive(c))

                check:addeventhandler(function (e)
                    data.setcategoryactive(c, e=='toggle-on')
                    manager.reloadcategories(false)
                end, 'toggle-on', 'toggle-off')

                box:spacing(5)
                box:pushback(check, 'middle', false)
                box:pushback(btn, 'middle', false)

                btn:addeventhandler(function() self:_runhandlers(c.typeid) end, 'click-left')

                self.box:pushback(box, 'start', false)
            end
        end
    end
end

function BreadCrumbBox:_runhandlers(newpath)
    for i,h in ipairs(self.handlers) do
        h(newpath)
    end
end

function BreadCrumbBox:addeventhandler(handler)
    table.insert(self.handlers, handler)

    return #self.handlers
end

function BreadCrumbBox:removeeventhandler(id)
    table.remove(self.handlers, id)
end

-- a category/row within the window
local Category = {}
Category.__index = Category

function Category.new(category)
    local c = {}
    c.category = category
    c.label = ui.text(category.displayname or category.type, ui.color('text'), ui.fonts.monospace)

    if (category.isseparator or 0) == 0 then
        c.check = checkbox()

        if data.iscategoryactive(category) then
            c.check:checkstate(true)
        end

        c.check:addeventhandler(function()
            data.setcategoryactive(category, false)
            manager.reloadcategories(false)
        end, 'toggle-off')
        c.check:addeventhandler(function()
            data.setcategoryactive(category, true )
            manager.reloadcategories(false)
        end, 'toggle-on')

        if category:childcount() > 0 then
            c.btn = childrenbutton()
        end
    end

    setmetatable(c, Category)

    return c
end

function Category:attachtogrid(grid, row)
    if self.check then
        grid:attach(self.check, row, 1, 1, 1, 'middle','middle')
    end
    grid:attach(self.label, row, 2, 1, 1, 'fill', 'middle')
    if self.btn then
        grid:attach(self.btn, row, 3, 1, 1, 'middle', 'middle')
    end
end

local CategoryManager = {}
CategoryManager.__index = CategoryManager

function CategoryManager.new()
    local cm = { }

    setmetatable(cm, CategoryManager)

    cm:setupwin()
    cm:setupmenu()

    if settings:get('categoryManager.window.visible') then
        cm:show()
    end

    return cm
end

function CategoryManager:setupwin()
    self.win = ui.window('Markers', 20, 20)
    self.win:resizable(true)
    self.win:settings(settings, 'categoryManager.window')

    self.outerbox = ui.box('vertical')
    self.outerbox:paddingleft(5)
    self.outerbox:paddingright(5)
    self.outerbox:paddingtop(5)
    self.outerbox:paddingbottom(5)
    self.outerbox:spacing(2)
    self.outerbox:alignment('start')
    self.win:child(self.outerbox)

    self.breadcrumbs = BreadCrumbBox.new()

    self.breadcrumbs:addeventhandler(function(newpath)
        settings:set('categoryManager.path', newpath)
        self:update()
    end)

    self.categoryscroll = ui.scrollview()

    self.outerbox:pushback(self.breadcrumbs.box, 'fill', false)
    self.outerbox:pushback(ui.separator('horizontal'), 'fill', false)
    self.outerbox:pushback(self.categoryscroll, 'fill', true)
    self.outerbox:pushback(ui.separator('horizontal'), 'fill', false)

    self.inmapcheck = checkbox()
    self.inmapcheck:checkstate(settings:get('categoryManager.onlyShowCategoriesInMap'))
    self.inmapcheck:addeventhandler(function(event) self:showinmaptoggle(event) end,'toggle-on', 'toggle-off')

    self.inmapbox = ui.box('horizontal')
    self.inmapbox:spacing(5)
    self.inmapbox:pushback(self.inmapcheck, 'middle', false)
    self.inmapbox:pushback(ui.text('Only show categories in map', ui.color('text'), ui.fonts.regular), 'middle', false)
    self.outerbox:pushback(self.inmapbox, 'fill', false)

    self.outerbox:pushback(ui.separator('horizontal'), 'fill', false)

    self.buttonbox = ui.box('horizontal')
    self.buttonbox:spacing(2)
    self.buttonbox:alignment('end')
    self.outerbox:pushback(self.buttonbox, 'fill', false)

    self.reloadbtn = textbutton('Reload')
    self.buttonbox:pushback(self.reloadbtn, 'fill', false)

    self.closebtn = textbutton('Close')
    self.buttonbox:pushback(self.closebtn, 'fill', false)

    self.reloadbtn:addeventhandler(function() self:update() end, 'click-left')
    self.closebtn:addeventhandler(function() self:oncloseclick() end, 'click-left')
end

function CategoryManager:showinmaptoggle(event)
    local showinmap = event=='toggle-on'

    settings:set('categoryManager.onlyShowCategoriesInMap', showinmap)
    self:update()
end

function CategoryManager:show()
    self.win:show()
    settings:set('categoryManager.window.visible', true)
    self:update()
end

function CategoryManager:hide()
    self.win:hide()
    settings:set('categoryManager.window.visible', false)
end

function CategoryManager:setupmenu()
    self.overlaymenu = {
        rootitem = ui.textmenuitem('Markers', ui.color('text'), overlay_menu.font),
        menu = ui.menu(),
        catmanager = ui.textmenuitem('Manage', ui.color('text'), overlay_menu.font),
        settings = ui.textmenuitem('Settings', ui.color('text'), overlay_menu.font),
    }

    self.overlaymenu.rootitem:submenu(self.overlaymenu.menu)
    self.overlaymenu.menu:pushback(self.overlaymenu.catmanager)
    self.overlaymenu.menu:pushback(self.overlaymenu.settings)

    overlay_menu.additem(self.overlaymenu.rootitem)

    self.overlaymenu.catmanager:addeventhandler(function() self:oncatmanagerbtnclick() end, 'click-left')

    self:setmenuicons()
end

function CategoryManager:oncatmanagerbtnclick()
    if settings:get('categoryManager.window.visible') then
        self:hide()
    else
        self:show()
    end
    self:setmenuicons()
end

function CategoryManager:setmenuicons()
    if settings:get('categoryManager.window.visible') then
        self.overlaymenu.catmanager:icon(overlay_menu.visible_icon)
    else
        self.overlaymenu.catmanager:icon(overlay_menu.hidden_icon)
    end
end

function CategoryManager:oncloseclick()
    settings:set('categoryManager.window.visible', false)
    self.win:hide()

    self:setmenuicons()
end

function CategoryManager:getcategoriesforpath(path)
    local mapid = ml.context.mapid()
    local onlyinmap = settings:get('categoryManager.onlyShowCategoriesInMap')
    local cats = {}

    if path=='' then
        for packpath, pack in pairs(manager.markerpacks) do
            for category in pack:toplevelcategoriesiter() do
                -- we want categories that are separators OR those that have
                -- markers in the current map (if that setting is on)
                if onlyinmap and not (category.isseparator==1) and category:hasmarkersinmap(mapid, true) then
                    table.insert(cats, category)
                elseif not onlyinmap or category.isseparator==1 then
                    table.insert(cats, category)
                end
            end
        end
    else
        for packpath, pack in pairs(manager.markerpacks) do
            local category = pack:category(path)

            if category then
                for i, child in ipairs(category:children()) do
                    if onlyinmap and child.isseparator~=1 and child:hasmarkersinmap(mapid, true) then
                        table.insert(cats, child)
                    elseif not onlyinmap or child.isseparator==1 then
                        table.insert(cats, child)
                    end
                end
            end
        end
    end

    return cats
end

function CategoryManager:update()
    overlay.logdebug("Updating categories...")

    self.breadcrumbs:update()

    if settings:get('categoryManager.onlyShowCategoriesInMap') and ml.context.mapid()==0 then
        local txt = ui.text('(invalid map)', ui.color('text'), ui.fonts.regular)
        self.categoryscroll:child(txt)

        return
    end

    local parent = settings:get('categoryManager.path')

    local cats = self:getcategoriesforpath(parent)

    if #cats == 0 then
        local txt = ui.text('(none)', ui.color('text'), ui.fonts.regular)
        self.categoryscroll:child(txt)

        return
    end

    local catgrid = ui.grid(#cats, 3)
    catgrid:colspacing(5)
    catgrid:rowspacing(3)

    for i, category in ipairs(cats) do
        local cat = Category.new(category)

        if cat.btn then
            cat.btn:addeventhandler(function()
                settings:set('categoryManager.path', category.typeid)
                overlay.logdebug(string.format('Changed to %s, reloading...', category.typeid))
                self:update()
            end, 'click-left')
        end

        cat:attachtogrid(catgrid, i)
    end

    self.categoryscroll:child(catgrid)
end

function CategoryManager:onmapchanged()
    local onlyinmap = settings:get('categoryManager.onlyShowCategoriesInMap')

    if onlyinmap then
        self:update()
    end
end

overlay.addeventhandler('startup', function()
    M._cm = CategoryManager.new()
end)

overlay.addeventhandler('mumble-link-map-changed', function(event) M._cm:onmapchanged() end)

return M
