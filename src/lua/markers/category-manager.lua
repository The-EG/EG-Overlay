local overlay = require 'eg-overlay'
local settings = require 'markers.settings'
local manager = require 'markers.manager'
local ui = require 'eg-overlay-ui'
local uih = require 'ui-helpers'
local ml = require 'mumble-link'

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

local categoryscroll = ui.scrollview()
local categorybox = ui.box('vertical')
categorybox:pack_end(categoryscroll, true, 'fill')
outerbox:pack_end(categorybox, true, 'fill')

outerbox:pack_end(ui.separator('horizontal'), false, 'fill')

local inmapbox = ui.box('horizontal')
local inmapcheck = uih.checkbox()
local inmaptext = uih.text('Only show categories in map')

inmapcheck:state(settings:get('categoryManager.onlyShowCategoriesInMap'))

inmapbox:spacing(10)
inmapbox:pack_end(inmapcheck)
inmapbox:pack_end(inmaptext)

outerbox:pack_end(inmapbox)

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

local function doreloadcats()
    categorybox:pop_end()
    categorybox:pack_end(workingmsg)
    manager.reloadcategories()
    categorybox:pop_end()
    categorybox:pack_end(categoryscroll, true, 'fill')
end

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
                        cb:state(c:active())

                        cb:addeventhandler(function(event)
                            if event~='toggle-on' and event~='toggle-off' then return end

                            c:active(event=='toggle-on')
                            doreloadcats()                            
                        end)

                        local btn = shortbtn(c.displayname)

                        box:pack_end(btn, true, 'fill')
                        pathbox:pack_end(box)

                        btn:addeventhandler(function(event)
                            if event~='click-left' then return end
                            
                            settings:set('categoryManager.path', c.typeid)
                            updatecategories()
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
            check:state(cat:active())
            catgrid:attach(check, i, 1, 1, 1, 'start', 'middle')

            check:addeventhandler(function(event)
                if event~='toggle-on' and event~='toggle-off' then return end

                cat:active(event=='toggle-on')
                doreloadcats()                
            end)
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
                    updatecategories()
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

overlay.addeventhandler('mumble-link-map-changed', function()
    updatecategories()
end)

return M
