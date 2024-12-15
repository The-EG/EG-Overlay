require 'mumble-link-events'

local overlay = require 'eg-overlay'
local logger = require 'logger'
local ml = require 'mumble-link'
local ui = require 'eg-overlay-ui'
local uih = require 'ui-helpers'
local mp = require 'markers.package'
local o3d = require 'eg-overlay-3d'
local utils = require 'utils'
local gw2 = require 'gw2'

local settings = require 'markers.settings'

local M = {}

-- marker packs
M.packs = {}

local log = logger.logger:new('markers.manager')

local mlavailable = false

local texturearray = o3d.texturemap()

-- marker points
local spritearray    = o3d.spritelist(texturearray, 'world')
local mapspritearray = o3d.spritelist(texturearray, 'map'  )

-- marker trails
local traillist    = o3d.traillist(texturearray, 'world')
local maptraillist = o3d.traillist(texturearray, 'map'  )

-- marker information used to show tooltips
local tooltipmarkers = {
    map = {},
    world = {}
}

local coordconverter = nil

-- a list of typeids that have markers or trails in one of the lists/arrays above
-- this is structured as markerpack -> typeid
local activetypeids = {}

local tooltipwin = ui.window('Marker Info', 50, 50)
tooltipwin:titlebar(false)

local function addcategorymarkers(category)
    local packpath = category.markerpack.path
    local typeid = category.typeid

    local markercount = 0
    for m in category:markersinmapiter(ml.mapid) do
        local color = uih.rgbtorgba(m.color or 0xFFFFFF)
        uih.colorsetalphaf(color, m.alpha or 1.0)

        local attrs = {
            x = utils.meterstoinches(m.xpos),
            y = utils.meterstoinches(m.ypos + (m.heightoffset or 1.5)),
            z = utils.meterstoinches(m.zpos),
            size = (m.iconsize or 1.0) * 80,
            fadefar = m.fadefar,
            fadenear = m.fadenear,
            color = color,
            tags = {
                pack = packpath,
                typeid = typeid,
                markerid = m.id
            }
        }

        local texturename = m.iconfile

        if not texturearray:has(texturename) then
            local texture = category.markerpack:datafile(texturename)
            local texdata = texture:data()

            texturearray:add(texturename, texdata)
        end        
        
        spritearray:add(texturename, attrs)

        if (
            (m.minimapvisibility==1 or m.minimapvisibility==nil) or
            (m.mapvisibility==1 or m.mapvisibility==nil)
        ) then
            -- only show in world tooltips for markers show in map
            table.insert(tooltipmarkers.world, {
                x = utils.meterstoinches(m.xpos),
                y = utils.meterstoinches(m.ypos + (m.heightoffset or 1.5)),
                z = utils.meterstoinches(m.zpos),
                size = (m.iconsize or 1.0) * 80,
                fadefar = m.fadefar or -1,
                marker = m
            })

            -- add it to the (mini)map
            local cx, cy = coordconverter:map2continent(utils.meterstoinches(m.xpos), utils.meterstoinches(m.zpos))

            table.insert(tooltipmarkers.map, {
                x = cx, y = cy, marker = m, size = m.mapdisplaysize
            })

            local mapattrs = {
                x = cx, y = cy,
                size = m.mapdisplaysize or 30,
                color = color,
                tags = {
                    pack = packpath,
                    typeid = typeid,
                    markerid = m.id
                }
            }
            mapspritearray:add(texturename, mapattrs) 
        end

        markercount = markercount + 1
    end

    if markercount > 0 then 
        log:debug("Loaded %d markers from %s", markercount, typeid)
    end
end

local function addcategorytrails(category)
    local packpath = category.markerpack.path
    local typeid = category.typeid

    local trailcount = 0
    for t in category:trailsinmapiter(ml.mapid) do
        local segments = {}

        local coords = {}
        for x,y,z in t:pointsiter() do
            if x==0.0 and y==0.0 and z==0.0 then
                -- a special case that some trails use to separate segments
                -- some trails have little 1 coordinate segments, which I assume
                -- must be erroneous and are probably ignored by TaCO and Blish
                -- so we'll ignore them too ¯\_(ツ)_/¯
                if #coords > 1 then table.insert(segments, coords) end
                coords = {}
            else
                table.insert(coords, {
                    utils.meterstoinches(x),
                    utils.meterstoinches(y),
                    utils.meterstoinches(z)
                })
            end
        end
        if #coords > 1 then table.insert(segments, coords) end

        local texturename = t.texture

        local color = uih.rgbtorgba(t.color or 0xFFFFFF)
        uih.colorsetalphaf(color, t.alpha or 1.0)

        if not texturearray:has(texturename) then
            local texturedf = category.markerpack:datafile(texturename)
            texturearray:add(t.texture, texturedf:data())
        end

        -- show this one in the (mini)map
        if (
            (t.minimapvisibility==1 or t.minimapvisibility==nil) or
            (t.mapvisibility==1 or t.mapvisibility==nil)
        ) then
            for i, coords in ipairs(segments) do
                local contcoords = {}

                for i,c in ipairs(coords) do
                    local cx, cy = coordconverter:map2continent(c[1], c[3])
                    table.insert(contcoords, { cx, cy, 0.5 })
                end
                local mapattrs = {
                    color = color,
                    wall = false,
                    size = (t.trailscale or 1.0) * 20,
                    points = contcoords,
                    tags = {
                        pack = packpath,
                        typeid = typeid,
                        trailid = t.id
                    }
                }
                maptraillist:add(texturename, mapattrs)
            end            
        end
        
        if not (t.ingamevisibility==0) then
            for i, c in ipairs(segments) do
                local attrs = {
                    fadefar = t.fadefar,
                    fadenear = t.fadenear,
                    color = color,
                    wall = (t.iswall or 0)==1,
                    size = (t.trailscale or 1.0) * 40,
                    points = c,
                    tags = {
                        pack = packpath,
                        typeid = typeid,
                        trailid = t.id
                    }
                }
                traillist:add(texturename, attrs)
            end
        end
        trailcount = trailcount + 1
        coroutine.yield()
    end
    if trailcount > 0 then
        log:debug("Loaded %d trails from %s", trailcount, typeid)
    end
end

function M.loadmarkerpack(path)
    M.packs[path] = mp.markerpack:new(path)
end

function M.hidecategory(category)
    local packpath = category.markerpack.path
    local typeid = category.typeid
    local tags = {pack = packpath, typeid = typeid}

    spritearray:remove(tags)
    mapspritearray:remove(tags)
    traillist:remove(tags)
    maptraillist:remove(tags)

    
    local newmap = {}
    for i,mp in pairs(tooltipmarkers.map) do
        if mp.marker.category.typeid ~= typeid then table.insert(newmap, mp) end
    end
    tooltipmarkers.map = newmap

    local newworld = {}
    for i,mp in pairs(tooltipmarkers.world) do
        if mp.marker.category.typeid ~= typeid then table.insert(newworld, mp) end
    end
    tooltipmarkers.world = newworld

    for child in category:childreniter() do
        M.hidecategory(child)
    end

    activetypeids[packpath][typeid] = nil
end

function M.showcategory(category)
    local packpath = category.markerpack.path
    local typeid = category.typeid

    if not activetypeids[packpath] then
        activetypeids[packpath] = {}
    end

    if activetypeids[packpath][typeid] then return end

    addcategorymarkers(category)
    addcategorytrails(category)

    activetypeids[packpath][typeid] = true
end

function M.reloadcategories(clear)
    if clear then
        activetypeids = {}

        texturearray:clear()
        spritearray:clear()
        mapspritearray:clear()

        traillist:clear()
        maptraillist:clear()
    else
        for mp, cats in pairs(activetypeids) do
            local pack = M.packs[mp]
            for typeid,_ in pairs(cats) do
                local cat = pack:category(typeid, false)

                if not cat:ancestorsactive() then
                    M.hidecategory(cat)
                end
            end
        end
    end

    for name, mp in pairs(M.packs) do
        for cat in mp:categoriesinmapiter(ml.mapid) do
            if cat:ancestorsactive() then
                M.showcategory(cat)
            end
            coroutine.yield()
        end
    end
end

local function onmapchange()
    coordconverter = gw2.coordconverter:new()
    M.reloadcategories(true)
end

local function onstartup()
    M.packs = {}

    local packpaths = settings:get('markerpacks')

    if packpaths then
        log:info("Loading marker packs...")
        for i,packpath in ipairs(settings:get('markerpacks')) do
            M.packs[packpath] = mp.markerpack:open(packpath)
        end
    else
        log:warn("No markerpacks in settings.")
    end
   
    if ml.mapid ~= 0 then
        onmapchange()
    end


    if settings:get('categoryManager.window.show') then
        require('markers.category-manager').show()
    end
end

local function createtooltipinfo(marker, showabovebelow)
    local abovebelowsepcolor = overlay.settings():get('overlay.ui.colors.text')

    local playerpos = ml.avatarposition
    playerpos.x = utils.meterstoinches(playerpos.x)
    playerpos.y = utils.meterstoinches(playerpos.y)
    playerpos.z = utils.meterstoinches(playerpos.z)

    local markerx = utils.meterstoinches(marker.xpos)
    local markery = utils.meterstoinches(marker.ypos)
    local markerz = utils.meterstoinches(marker.zpos)

    local dist = math.sqrt(
        (playerpos.x - markerx)^2 +
        (playerpos.y - markery)^2 +
        (playerpos.z - markerz)^2
    )

    local box = ui.box('vertical')
    box:spacing(2)
    local diststr = string.format("%s away", utils.formatnumbercomma(math.floor(dist)))

    local distbox = ui.box('horizontal')
    distbox:padding(0,0,0,0)
    distbox:spacing(5)

    local abovebelowsep = ui.separator('vertical')
    abovebelowsep:color(abovebelowsepcolor)

    if showabovebelow and playerpos.y < markery and (markery - playerpos.y) >= 450 then
        distbox:pack_end(uih.text('\u{2191} Above', 0x01EFEFFF))
        distbox:pack_end(abovebelowsep)
    elseif showabovebelow and playerpos.y > markery and (playerpos.y - markery) >= 450 then
        distbox:pack_end(uih.text('\u{2193} Below', 0x00FF33FF))
        distbox:pack_end(abovebelowsep)
    end
    distbox:pack_end(uih.text(diststr))

    if marker['tip-name'] then
        box:pack_end(uih.text(marker['tip-name'], true))
    end

    local cat = marker.category
    local name = ''
    while cat do
        if name:len() > 30 then 
            name = '...> ' .. name
            break
        end

        if name:len() > 0 then
            name = cat.displayname .. '> ' .. name
        else
            name = cat.displayname
        end
        cat = cat:parent()
    end
    box:pack_end(uih.text(name, true))
    
    box:pack_end(ui.separator('horizontal'), false, 'fill')
    box:pack_end(distbox)

    if marker['tip-description'] then
        box:pack_end(ui.separator('horizontal'), false, 'fill')
        local desc = marker['tip-description']
        desc = string.gsub(desc, '%. ', '\n')
        box:pack_end(uih.text(desc))
    end
    
    if marker.info then
        box:pack_end(ui.separator('horizontal'), false, 'fill')
        box:pack_end(uih.text(marker.info))
    end

    local props = {'guid', 'behavior'}
    if #props > 0 then
        box:pack_end(ui.separator('horizontal'), false, 'fill')
        local propsgrid = ui.grid(#props, 2)
        propsgrid:rowspacing(2)
        propsgrid:colspacing(5)
        for i, prop in ipairs(props) do
            local nm = uih.text(prop, true)
            local val = uih.text(marker[prop] or '(nil)')
            propsgrid:attach(nm, i, 1)
            propsgrid:attach(val, i, 2)
        end
        box:pack_end(propsgrid)
    end

    return box
end

local function buildtooltipinfos(tooltipmps, showabovebelow)
    local box = ui.box('vertical')

    box:padding(5,5,2,2)
    box:spacing(10)

    for i,mp in ipairs(tooltipmps) do
        if i > 1 then
            box:pack_end(ui.separator('horizontal'), false, 'fill')
        end
        box:pack_end(createtooltipinfo(mp.marker, showabovebelow), false, 'fill')
    end

    tooltipwin:set_child(box)
end

local function drawtooltip()
    tooltipwin:hide()

    if not settings:get('showTooltips') or ml.incombat then return end

    local mousecx, mousecy = o3d.mousepointermapcoords()

    local playerpos = ml.avatarposition
    playerpos.x = utils.meterstoinches(playerpos.x)
    playerpos.y = utils.meterstoinches(playerpos.y)
    playerpos.z = utils.meterstoinches(playerpos.z)

    local tooltipmps = {}

    if mousecx then
        for i, mp in ipairs(tooltipmarkers.map) do
            local mpdistsq = (
                (mousecx - mp.x)^2 +
                (mousecy - mp.y)^2
            )
            local searchdistsq = ((mp.size or 20)/2.0)^2
            if mpdistsq < searchdistsq then
                table.insert(tooltipmps, mp)
            end
        end
    end

    if #tooltipmps > 0 then
        buildtooltipinfos(tooltipmps, true)
        local mx, my = ui.mouseposition()
        tooltipwin:position(mx - 10, my - 30)
        tooltipwin:hanchor(1)
        tooltipwin:vanchor(1)
        tooltipwin:show()

        return
    end

    if ml.mapopen then return end

    -- gather up all of the markers that are near the mouse cursor
    -- be sure to use only attributes that are cached, DO NOT query
    -- properties directly on the marker. That will perform a sqlite query
    -- for each marker for every frame. SLOW
    for i, mp in ipairs(tooltipmarkers.world) do
        local isfaded = false
        local mpdsq = (
            (playerpos.x - mp.x)^2 +
            (playerpos.y - mp.y)^2 +
            (playerpos.z - mp.z)^2
        )
        if mp.fadefar > 0 then
            local fadefarsq = mp.fadefar^2
            if mpdsq > fadefarsq then isfaded = true end
        end

        if not isfaded and mpdsq < 10000^2 then
            if o3d.mousepointsat(mp.x, mp.y, mp.z, mp.size/2.0) then
                table.insert(tooltipmps, mp)
            end
        end
    end

    if #tooltipmps > 0 then
        buildtooltipinfos(tooltipmps, false)

        local mx, my = ui.mouseposition()

        tooltipwin:position(mx + 30, my)
        tooltipwin:hanchor(-1)
        tooltipwin:vanchor(-1)
        tooltipwin:show()
    end
end

local function drawmarkers()
    if not settings:get('drawMarkers') or not mlavailable then return end

    maptraillist:draw()
    mapspritearray:draw()

    if ml.mapopen then return end

    traillist:draw()
    spritearray:draw()
end

local function mlavailablechanged(event)
    if event=='mumble-link-available' then
        mlavailable = true
    else
        mlavailable = false
    end
end

local function ondraw()
    drawtooltip()
    drawmarkers()
end

overlay.addeventhandler('startup', onstartup)
overlay.addeventhandler('draw-3d', ondraw)

overlay.addeventhandler('mumble-link-map-changed', onmapchange)
overlay.addeventhandler('mumble-link-available'  , mlavailablechanged)
overlay.addeventhandler('mumble-link-unavailable', mlavailablechanged)

return M
