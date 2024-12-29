local overlay = require 'eg-overlay'
local settings = require 'markers.settings'
local manager = require 'markers.manager'
local ui = require 'eg-overlay-ui'
local uih = require 'ui-helpers'
local ml = require 'mumble-link'
local data = require 'markers.data'
local logger = require 'logger'

local log = logger.logger:new('markers.cat-mgr')

local M = {}

-- The path of the categories displayed. This is a category typeid, of which
-- all child categories are displayed.
settings:setdefault('categoryManager.path', '')
settings:setdefault('categoryManager.onlyShowCategoriesInMap', false)

settings:setdefault('categoryManager.window.x', 200)
settings:setdefault('categoryManager.window.y', 50)
settings:setdefault('categoryManager.window.width', 300)
settings:setdefault('categoryManager.window.height', 600)
settings:setdefault('categoryManager.window.show', false)

local function shortbtn(text)
    local btn = ui.button()
    local box = ui.box('vertical')
    local txt = uih.text(text)
    box:padding(5,5,0,0)
    box:pack_end(txt)
    btn:set_child(box)

    return btn
end

local win = ui.window('Manage Markers', 20, 20)
win:min_size(300, 300)
win:resizable(true)
win:settings(settings, 'categoryManager.window')

local outerbox = ui.box('vertical')
win:set_child(outerbox)
outerbox:padding(5, 5, 5, 5)
outerbox:spacing(2)
outerbox:align('start')

local pathbox = ui.box('vertical')

pathbox:padding(0, 0, 2, 2)
pathbox:spacing(2)

local homebtnbox = ui.box('horizontal')
homebtnbox:spacing(5)
homebtnbox:padding(3,0,0,0)
local homebtn = shortbtn('All markers')
local homecheck = uih.checkbox()
homebtnbox:pack_end(homecheck, false, 'middle')
homebtnbox:pack_end(homebtn)

local workingmsg = uih.text('Please wait...')

homecheck:state(settings:get('drawMarkers'))

homecheck:addeventhandler(function(event)
    if event~='toggle-on' and event~='toggle-off' then return end

    settings:set('drawMarkers', event=='toggle-on')
end)

pathbox:pack_end(homebtnbox)

outerbox:pack_end(pathbox, false, 'fill')

outerbox:pack_end(ui.separator('horizontal'), false, 'fill')

local contextmenu = {
    menu = ui.menu(),
    cat = nil,
}

local function contextmenuitem(child, enabled, cb)
    local mi = ui.menu_item()
    if enabled==nil then enabled = true end
    mi:enabled(enabled)
    mi:set_child(child)
    contextmenu.menu:add_item(mi)

    if cb then
        mi:addeventhandler(cb)
    end
    
    return mi
end

local function contextmenusep()
    return contextmenuitem(ui.separator('horizontal'), false)
end

local function contextmenutext(text, enabled, cb)
    return contextmenuitem(uih.text(text), enabled, cb)
end

contextmenu.parentmi = contextmenutext('Parent name', false)

contextmenutext('Enable all', true, function(event)
    if event~='click-left' then return end
    contextmenu.menu:hide()
    M.setallactive(true)
end)

contextmenutext('Disable all', true, function(event)
    if event~='click-left' then return end
    contextmenu.menu:hide()
    M.setallactive(false)
end)

contextmenusep()
contextmenu.catmi = contextmenutext('Category Name', false)

contextmenutext('Enable all children', true, function(event)
    if event~='click-left' then return end
    contextmenu.menu:hide()
    M.enableallchildren(contextmenu.cat)
end)

contextmenutext('Disable others', true, function(event)
   if event~='click-left' then return end
   contextmenu.menu:hide()
   M.disableothers(contextmenu.cat)
end)

-- contextmenutext('Clear all activations', true, function(event)
--     if event~='click-left' then return end
--     log:debug('%s clear all activations', contextmenu.cat.typeid)
--     contextmenu.menu:hide()
-- end)

local function showcontextmenu(category)
    local parentname = '(Top Level)'

    local parent = category:parent()
    if parent then parentname = parent.displayname end

    contextmenu.catmi:set_child(uih.text(category.displayname, true))
    contextmenu.parentmi:set_child(uih.text(parentname, true))
    contextmenu.cat = category
    
    local x,y = ui.mouseposition()
    contextmenu.menu:show(x,y)
end

local categoryscroll = ui.scrollview()
local categorybox = ui.box('vertical')
categorybox:pack_end(categoryscroll, true, 'fill')
outerbox:pack_end(categorybox, true, 'fill')

outerbox:pack_end(ui.separator('horizontal'), false, 'fill')

local inmapbox = ui.box('horizontal')
local inmapcheck = uih.checkbox()
local inmaptext = uih.text('Only show categories in map')

local reloadbtn = uih.text_button('Reload Markers')

inmapcheck:state(settings:get('categoryManager.onlyShowCategoriesInMap'))

inmapbox:spacing(10)
inmapbox:pack_end(inmapcheck)
inmapbox:pack_end(inmaptext)

outerbox:pack_end(inmapbox)
outerbox:pack_end(reloadbtn, false, 'fill')

local closebtn = uih.text_button('Close')
outerbox:pack_end(closebtn, false, 'fill')

closebtn:addeventhandler(function(event)
    if event=='click-left' then
        M.hide()
    end
end)

local function childrenbutton()
    local t = uih.text('\u{21AA}')
    local box = ui.box('horizontal')
    box:padding(5,5,0,0)
    local btn = ui.button()
    btn:set_child(box)
    box:align('middle')
    box:pack_end(t, false,'start')

    return btn
end

local function doreloadcats(full)
    categorybox:pop_end()
    categorybox:pack_end(workingmsg)
    manager.reloadcategories(full)
    categorybox:pop_end()
    categorybox:pack_end(categoryscroll, true, 'fill')
end

reloadbtn:addeventhandler(function(event)
    if event=='click-left' then
        doreloadcats(true)
    end
end)

local function updatecategories()
    local parent = settings:get('categoryManager.path')
    local onlyinmap = settings:get('categoryManager.onlyShowCategoriesInMap')
    local mapid = ml.mapid

    local cats = {}

    categoryscroll:set_child(uih.text('(working)'))

    while pathbox:item_count() > 1 do pathbox:pop_end() end

    coroutine.yield()

    if parent~='' then
        for name, mp in pairs(manager.packs) do
            local category = mp:category(parent)

            if category then
                if pathbox:item_count() == 1 then
                    local pathcats = {}

                    local c = category
                    while c do
                        table.insert(pathcats,1,c)
                        c = c:parent()
                    end

                    for i, c in ipairs(pathcats) do
                        local lbl = ' '
                        for s = 1, pathbox:item_count()-1 do
                            lbl = lbl .. '   '
                        end

                        lbl = lbl .. '\u{2570}\u{2500}'
                        local box = ui.box('horizontal')
                        box:spacing(3)
                        box:pack_end(uih.monospace_text(lbl), false, 'middle')

                        local cb = uih.checkbox()
                        box:pack_end(cb, false, 'middle')
                        cb:state(data.iscategoryactive(c))

                        cb:addeventhandler(function(event)
                            if event~='toggle-on' and event~='toggle-off' then return end

                            data.setcategoryactive(c, event=='toggle-on')
                            log:debug("%s toggled, reloading...", c.typeid)
                            doreloadcats()
                            log:debug("done.")
                        end)

                        local btn = shortbtn(c.displayname)

                        box:pack_end(btn, true, 'fill')
                        pathbox:pack_end(box)

                        btn:addeventhandler(function(event)
                            if event~='click-left' then return end
                            
                            settings:set('categoryManager.path', c.typeid)
                            log:debug("Changed to %s, reloading...", c.typeid)
                            updatecategories()
                            log:debug("done.")
                        end)
                    end
                end

                for child in category:childreniter() do
                    if onlyinmap and not (child.isseparator==1) then
                        if child:hasmarkersinmap(mapid, true) then
                            coroutine.yield()
                            table.insert(cats, child)
                        end
                    else
                        table.insert(cats, child)
                    end
                end
            end
        end
    else
        for name, mp in pairs(manager.packs) do
            for category in mp:toplevelcategoriesiter() do
                if onlyinmap and not (category.isseparator==1) then 
                    if category:hasmarkersinmap(mapid, true) then
                        table.insert(cats, category)
                    end
                else
                    table.insert(cats, category)
                end
            end
        end
    end

    if #cats==0 then
        categoryscroll:set_child(uih.text('(none)'))
        return
    end

    local catgrid = ui.grid(#cats, 3)
    catgrid:rowspacing(2)
    catgrid:colspacing(10)
    categoryscroll:set_child(catgrid)
    
    for i, cat in ipairs(cats) do
        local text = uih.monospace_text(cat.displayname)

        if cat.isseparator~=1 then
            local check = uih.checkbox()
            check:state(data.iscategoryactive(cat))
            catgrid:attach(check, i, 1, 1, 1, 'start', 'middle')

            check:addeventhandler(function(event)
                if event~='toggle-on' and event~='toggle-off' then return end

                data.setcategoryactive(cat, event=='toggle-on')
                log:debug("%s toggled, reloading...", cat.typeid)
                doreloadcats()
                log:debug("done.")
            end)

            text:addeventhandler(function(event)
                if event=='btn-up-left' then
                    check:state(not check:state())
                    data.setcategoryactive(cat, check:state())
                    log:debug("%s toggled, reloading...", cat.typeid)
                    doreloadcats()
                    log:debug("done.")
                elseif event=='btn-up-right' then
                    showcontextmenu(cat)
                end
            end)
            text:events(true)
        end
        local textalign = 'fill'
        -- center align separator items if they do not start with spaces
        -- spaces indicate the author already centered the text
        if cat.isseparator==1 and string.sub(cat.displayname, 1, 1)~=' ' then
            textalign = 'middle'
        end
        catgrid:attach(text , i, 2, 1, 1, textalign, 'middle')

        if cat:childcount() > 0 then
            local btn = childrenbutton()
            btn:addeventhandler(function(event)
                if event=='click-left' then
                    settings:set('categoryManager.path', cat.typeid)
                    log:debug("Changed to %s, reloading...", cat.typeid)
                    updatecategories()
                    log:debug("done.")
                end
            end)

            catgrid:attach(btn, i, 3)
        end
    end
end

homebtn:addeventhandler(function(event)
    if event~='click-left' then return end
    
    settings:set('categoryManager.path', '')
    while pathbox:item_count() > 1 do
        pathbox:pop_end()
    end
    updatecategories()
end)


inmapcheck:addeventhandler(function(event)
    if event~='toggle-on' and event~='toggle-off' then return end

    settings:set('categoryManager.onlyShowCategoriesInMap', event=='toggle-on')
    updatecategories()
end)


if settings:get('categoryManager.window.show') then
    win:show()
end

function M.show()
    win:show()
    coroutine.yield()
    updatecategories()
    settings:set('categoryManager.window.show', true)
end

function M.hide()
    win:hide()
    settings:set('categoryManager.window.show', false)
end

function M.disableothers(category)
    local parent = settings:get('categoryManager.path')
    local onlyinmap = settings:get('categoryManager.onlyShowCategoriesInMap')
    local mapid = ml.mapid

    local cats = {}

    if parent~='' then
        for name, mp in pairs(manager.packs) do
            local category = mp:category(parent)

            if category then
                for child in category:childreniter() do
                    if onlyinmap and not (child.isseparator==1) then
                        if child:hasmarkersinmap(mapid, true) then
                            coroutine.yield()
                            table.insert(cats, child)
                        end
                    else
                        table.insert(cats, child)
                    end
                end
            end
        end
    else
        for name, mp in pairs(manager.packs) do
            for category in mp:toplevelcategoriesiter() do
                if onlyinmap and not (category.isseparator==1) then 
                    if category:hasmarkersinmap(mapid, true) then
                        table.insert(cats, category)
                    end
                else
                    table.insert(cats, category)
                end
            end
        end
    end

    for i,c in ipairs(cats) do
        if c.typeid==category.typeid then
            data.setcategoryactive(c, true)
        else
            data.setcategoryactive(c, false)
        end
    end

    log:debug("Disabled others, reloading...")
    updatecategories()
    doreloadcats()
    log:debug('done.')
end

-- disables all categories at the current path
function M.setallactive(active)
    local parent = settings:get('categoryManager.path')
    local onlyinmap = settings:get('categoryManager.onlyShowCategoriesInMap')
    local mapid = ml.mapid

    local cats = {}

    if parent~='' then
        for name, mp in pairs(manager.packs) do
            local category = mp:category(parent)

            if category then
                for child in category:childreniter() do
                    if onlyinmap and not (child.isseparator==1) then
                        if child:hasmarkersinmap(mapid, true) then
                            coroutine.yield()
                            table.insert(cats, child)
                        end
                    else
                        table.insert(cats, child)
                    end
                end
            end
        end
    else
        for name, mp in pairs(manager.packs) do
            for category in mp:toplevelcategoriesiter() do
                if onlyinmap and not (category.isseparator==1) then 
                    if category:hasmarkersinmap(mapid, true) then
                        table.insert(cats, category)
                    end
                else
                    table.insert(cats, category)
                end
            end
        end
    end

    for i,c in ipairs(cats) do
        data.setcategoryactive(c, active)
    end

    if active then
        log:debug("Enabled all categories, reloading...")
    else
        log:debug("Disabled all categories, reloading...")
    end
    updatecategories()
    doreloadcats()
    log:debug('done.')
end

function M.enableallchildren(category)
    local onlyinmap = settings:get('categoryManager.onlyShowCategoriesInMap')
    local mapid = ml.mapid

    local function enablechildren(cat)
        if cat.isseparator~=1 then
            if (onlyinmap and cat:hasmarkersinmap(mapid, true)) or not onlyinmap then
                data.setcategoryactive(cat,  true)
                for i,child in ipairs(cat:children()) do
                    enablechildren(child)
                end
            end
        end
    end

    enablechildren(category)
    log:debug("Enabled all children of %s, reloading...", category.typeid)
    updatecategories()
    doreloadcats()
    log:debug("done.")
end

overlay.addeventhandler('mumble-link-map-changed', function()
    updatecategories()
end)

return M
