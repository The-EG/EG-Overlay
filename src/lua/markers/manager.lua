-- EG-Overlay
-- Copyright (c) 2025 Taylor Talkington
-- SPDX-License-Identifier: MIT
require 'mumble-link-events'

local overlay = require 'overlay'
local ml = require 'mumble-link'
local ui = require 'ui'
local mp = require 'markers.package'
local md = require 'markers.data'
local ms = require 'markers.settings'
local gw2 = require 'gw2'
local dx = require 'dx'

ms:setdefault('defaultMarkerColor', 0xFFFFFF)
ms:setdefault('defaultMarkerAlpha', 1.0)
ms:setdefault('defaultHeightOffset', 1.5)
ms:setdefault('defaultIconSize', 1.0)
ms:setdefault('markerBaseSize', 80)
ms:setdefault('defaultMarkerMapDisplaySize', 20)

local M = {}

M.textures = dx.texturemap()

M.worldsprites = dx.spritelist(M.textures, 'world')
M.worldsprites:draw(false)

M.mapsprites = dx.spritelist(M.textures, 'map')
M.mapsprites:draw(false)

M.worldtrails = dx.traillist(M.textures, 'world')
M.worldtrails:draw(false)

M.maptrails = dx.traillist(M.textures, 'map')
M.maptrails:draw(false)

M.coordconv = {}

local function m2in(meters)
    return meters * 39.3701
end

-- a list of typeids that have markers or trails in one of the lists above
M.activetypeids = {}

M.markerpacks = {}

M.activatewin = {}

function M.activatewin:setupwin()
    self.win = ui.window('Activate Marker')
    self.btn = ui.button()
    self.box = ui.box('horizontal')
    self.icon = ui.text(ui.iconcodepoint('location_off'), ui.color('accentText'), ui.fonts.icon:tosizeperc(1.5))
    self.text = ui.text('Hide marker', ui.color('text'), ui.fonts.regular:tosizeperc(1.25))

    self.win:titlebar(false)
    self.win:child(self.btn)

    self.btn:child(self.box)

    self.box:paddingleft(5)
    self.box:paddingright(5)
    self.box:paddingtop(5)
    self.box:paddingbottom(5)
    self.box:spacing(5)

    self.box:pushback(self.icon, 'middle', false)
    self.box:pushback(self.text, 'middle', false)

    self.visible = false

    self.btn:addeventhandler(function()
        M.behaviormgr:activatemarker()
    end, 'click-left')
end

function M.activatewin:show()
    local w,h = ui.overlaysize()

    local x = math.floor(w / 2.0 + (w / 10))
    local y = math.floor(h / 2.0 - (h / 10))

    self.win:position(x, y)

    if not self.visible then
        self.win:show()
        self.visible = true
    end
end

function M.activatewin:hide()
    if self.visible then
        self.win:hide()
        self.visible = false
    end
end

M.activatewin:setupwin()

M.infowin = {}

function M.infowin:setup()
    self.win = ui.window('Marker Info')
    self.box = ui.box('vertical')

    self.win:child(self.box)
    self.win:titlebar(false)
    self.win:ignoremouse(true)
    self.win:bgcolor(0x0)
    self.win:bordercolor(0x0)

    self.visible = false
end

function M.infowin:show(text)
    self.box:clear()

    for i,msg in ipairs(overlay.splitstring(text, '&#xA;')) do
        if msg == '' then msg = ' ' end
        local txt = ui.text(msg, ui.color('accentText'), ui.fonts.regular:tosizeperc(1.5))
        self.box:pushback(txt, 'middle', false)
    end

    self.win:updatesize()

    local w,h = ui.overlaysize()

    local x = math.floor(w / 2.0 - (self.win:width() / 2.0))
    local y = math.floor(h / 2.0 - self.win:height() - (h / 5))

    if y < 0 then y = 20 end

    self.win:position(x, y)

    if not self.visible then
        self.win:show()
        self.visible = true
    end
end

function M.infowin:hide()
    if self.visible then
        self.visible = false
        self.win:hide()
    end
end

M.infowin:setup()

-- a centralized place to handle marker behaviors/activations
M.behaviormgr = {
    -- cached marker info (location, behavior type, info to display, etc.)
    -- indexed by guid
    markercache = {},

    -- a spatial index to make locating markers near the player efficient
    spatialindex = {},

    -- makers that have been activated in this map
    -- and should be shown again on map change
    mapguids = {},

    infoguid = nil,
    copyguid = nil,
}

function M.behaviormgr:clear()
    self.markercache = {}

    -- We use a spatial index to speed up checking which markers are near the
    -- player. This index consists of 'cells' that are 10 meters across. We use
    -- meters since that's what MumbleLink is giving us and that's what we
    -- expect the markers to be using too; no need to convert.
    --
    -- The cells are stored in Lua tables, the first level X, second Y, and third Z
    -- A cell can be indexed for any position by doing the following:
    -- x_ind = math.tointeger(math.floor(x / 10))
    -- y_ind = math.tointeger(math.floor(y / 10))
    -- z_ind = math.tointeger(math.floor(z / 10))
    -- cell = self.spatialindex[x_ind][y_ind][z_ind]
    --
    -- note: cells are only created if markers exist in those cells
    self.spatialindex = {}

    self.infoguid = nil
    self.copyguid = nil
end

function M.behaviormgr:addmarker(marker)
    local m = {
        guid = marker.guid,
        x = marker.xpos or 0,
        y = marker.ypos or 0,
        z = marker.zpos or 0,
        info = marker.info,
        inforange = marker.inforange or 2,
        behavior = marker.behavior or 0,
        displayname = marker.displayname,
        triggerrange = marker.triggerrange or 2,
        typeid = marker.typeid,
        autotrigger = (marker.autotrigger or 0) == 1,
        copy = marker.copy,
        copymessage = marker['copy-message'],
    }

    if not m.info and not m.copy and m.behavior==0 then return end

    local range = m.triggerrange

    if m.inforange > range then range = m.inforange end

    -- add a marker to each index cell that the marker's range covers
    -- this technically covers too many cells, in a cube shape, instead of
    -- a spherical shape, but this is much simpler to calculate

    local min_x_ind = math.tointeger(math.floor((m.x - range) / 10))
    local max_x_ind = math.tointeger(math.floor((m.x + range) / 10))

    local min_y_ind = math.tointeger(math.floor((m.y - range) / 10))
    local max_y_ind = math.tointeger(math.floor((m.y + range) / 10))

    local min_z_ind = math.tointeger(math.floor((m.z - range) / 10))
    local max_z_ind = math.tointeger(math.floor((m.z + range) / 10))

    for x_ind = min_x_ind,max_x_ind do
        for y_ind = min_y_ind,max_y_ind do
            for z_ind = min_z_ind,max_z_ind do
                if not self.spatialindex[x_ind] then self.spatialindex[x_ind] = {} end
                if not self.spatialindex[x_ind][y_ind] then self.spatialindex[x_ind][y_ind] = {} end
                if not self.spatialindex[x_ind][y_ind][z_ind] then self.spatialindex[x_ind][y_ind][z_ind] = {} end

                table.insert(self.spatialindex[x_ind][y_ind][z_ind], m.guid)
            end
        end
    end

    self.markercache[m.guid] = m
end

function M.behaviormgr:removecategory(typeid)
    local rmguids = {}

    for guid, m in pairs(self.markercache) do
        if m.typeid==typeid then
            table.insert(rmguids, guid)
        end
    end

    for i,guid in ipairs(rmguids) do
        self.markercache[guid] = nil
    end
end

function M.behaviormgr:update()
    self.closestguid = nil
    self.doactivate = false

    M.activatewin:hide()

    local px, py, pz = ml.avatarposition()

    local px_ind = math.tointeger(math.floor(px / 10))
    local py_ind = math.tointeger(math.floor(py / 10))
    local pz_ind = math.tointeger(math.floor(pz / 10))

    local closestguid = nil
    local closestdsqr = nil

    -- first check to see if our spatial index contains any markers in the same
    -- cell as the player
    if self.spatialindex[px_ind] and self.spatialindex[px_ind][py_ind] and self.spatialindex[px_ind][py_ind][pz_ind] then
        -- and then check each marker in that cell to see which is the closest
        for i, guid in ipairs(self.spatialindex[px_ind][py_ind][pz_ind]) do
            local m = self.markercache[guid]

            if not m then goto skipguid end

            local dsqr = (m.x - px)^2 + (m.y - py)^2 + (m.z - pz)^2

            if closestguid==nil or (dsqr < closestdsqr) then
                closestguid = guid
                closestdsqr = dsqr
            end

            ::skipguid::
        end
    end

    self.closestguid = closestguid

    if self.closestguid then
        self.closestdist = math.sqrt(closestdsqr)
    else
        self.closestdist = nil
        self.infoguid = nil
        self.copyguid = nil
        M.infowin:hide()
        return
    end

    local m = self.markercache[closestguid]

    if m.behavior == 0 and not m.info and not m.copy then return end

    if self.closestdist <= m.triggerrange then
        if not m.autotrigger and (m.behavior~=0 or m.copy) then
            M.activatewin:show()
            self.doactivate = true
        elseif m.autotrigger then
            self.doactivate = true
            self:activatemarker()
        end
    else
        self.copyguid = nil
    end

    if self.closestdist <= m.inforange and m.info then
        if self.infoguid ~= self.closestguid then
            M.infowin:show(m.info)
            self.infoguid = self.closestguid
        end
    else
        self.infoguid = nil
        M.infowin:hide()
    end
end

function M.behaviormgr:activatemarker()
    if not self.closestguid then return end
    if not self.doactivate then return end

    local m = self.markercache[self.closestguid]

    if not m then return end

    if m.behavior==0 then return end

    if m.behavior == 1 then
        table.insert(self.mapguids, m.guid)
        self.markercache[m.guid] = nil

        M.worldsprites:remove({guid = m.guid})
        M.mapsprites:remove({guid = m.guid})
        overlay.logdebug(string.format('%s activated (behavior = 1)', m.guid))
        self.closestguid = nil
    elseif m.behavior == 2 or m.behavior == 3 or m.behavior == 7 or m.behavior == 101 then
        md.activateguid(m.guid, ml.identity.name())
        overlay.logdebug(string.format('%s activated (behavior = %d)', m.guid, m.behavior))
        self.markercache[m.guid] = nil

        M.worldsprites:remove({guid = m.guid})
        M.mapsprites:remove({guid = m.guid})
        self.closestguid = nil
    elseif m.behavior == 6 then
        local instid = string.format('%d|%d', ml.context.mapid(), ml.context.shardid())
        md.activateguid(m.guid, instid)
        overlay.logdebug(string.format('%s activated (behavior = %d)', m.guid, m.behavior))
        self.markercache[m.guid] = nil

        M.worldsprites:remove({guid = m.guid})
        M.mapsprites:remove({guid = m.guid})
        self.closestguid = nil
    end
end

M.tooltipwin = {}
M.tooltipwin.fields = {
    {'guid'       , 'GUID'    },
    {'behavior'   , 'Behvaior'},
    {'info'       , 'Info'    },
    {'inforange'  , 'InfoRange'},
    {'triggerrange', 'TriggerRange'},
    {'copy'       , 'Copy' },
    {'copy-message', 'Copy-Message'},
    {'autotrigger', 'AutoTrigger'},
}
M.tooltipwin.cache = {}
M.tooltipwin.visible = false

function M.tooltipwin:createwin()
    self.win = ui.window("Markers Tooltip")
    self.win:titlebar(false)
    self.box = ui.box('vertical')
    self.win:child(M.tooltipwin.box)
    self.box:paddingleft(3)
    self.box:paddingright(3)
    self.box:paddingtop(3)
    self.box:paddingbottom(3)
    self.box:spacing(3)

    self.titletxt = ui.text('(Title)', ui.color('accentText'), ui.fonts.regular)
    self.pathtxt = ui.text('(Path)', 0xc7c7c7FF, ui.fonts.regular:tosizeperc(0.85))
    self.descriptiontxt = ui.text('(Description)', ui.color('text'), ui.fonts.regular)

    self.box:pushback(self.titletxt, 'start', false)
    self.box:pushback(self.pathtxt, 'start', false)

    self.box:pushback(ui.separator('horizontal'), 'fill', false)

    self.fieldgrid = ui.grid(#M.tooltipwin.fields, 2)
    self.fieldgrid:colspacing(5)
    self.fieldgrid:rowspacing(2)

    self.box:pushback(self.fieldgrid, 'fill', false)

    self.fieldtxt = {}

    for i, field in ipairs(M.tooltipwin.fields) do
        local name = field[1]
        local label = field[2]
        local lbltxt = ui.text(label, ui.color('accentText'), ui.fonts.regular)
        self.fieldtxt[name] = ui.text('(Value)', ui.color('text'), ui.fonts.monospace)
        self.fieldgrid:attach(lbltxt, i, 1, 1, 1, 'end', 'start')
        self.fieldgrid:attach(self.fieldtxt[name], i, 2, 1, 1, 'start', 'start')
    end
end

M.tooltipwin:createwin()

function M.tooltipwin:show()
    local mx, my = ui.mouseposition()

    self.win:position(mx + 30, my)
    if not self.visible then
        self.win:show()
        self.visible = true
    end
end

function M.tooltipwin:hide()
    if self.visible then
        self.win:hide()
        self.visible = false
    end
end

function M.tooltipwin:setmarker(markerpack, typeid, markerid)
    local marker = nil
    if M.tooltipwin.cache[markerpack] and M.tooltipwin.cache[markerpack][typeid] then
        marker = M.tooltipwin.cache[markerpack][typeid][markerid]
    end

    if not marker then
        local pack = M.markerpacks[markerpack]
        local cat = pack:category(typeid)

        marker = { db = mp.db, category = cat, id = markerid }
        setmetatable(marker, mp.marker)

        if not M.tooltipwin.cache[markerpack] then M.tooltipwin.cache[markerpack] = {} end
        if not M.tooltipwin.cache[markerpack][typeid] then M.tooltipwin.cache[markerpack][typeid] = {} end

        local mcache = {}

        for i, field in ipairs(self.fields) do
            local f = field[1]
            local val = marker[f] or '(none)'

            mcache[f] = val
        end

        mcache.description = marker['tip-description']
        mcache.title = marker['tip-name'] or marker.displayname

        if mcache.description then
            -- word wrap long descriptions

            local descwrap = ''
            local linelen = 0
            for str in mcache.description:gmatch('([^ ]+)') do
                if #str + linelen > 60 then
                    descwrap = descwrap .. '\n' .. str
                    linelen = #str
                else
                    if #descwrap > 0 then
                        descwrap = descwrap .. ' '
                        linelen = linelen + 1
                    end
                    descwrap = descwrap .. str
                    linelen = linelen + #str
                end
            end

            mcache.description = descwrap
        end

        local p = cat:parent()
        mcache.path = p.displayname
        p = p:parent()

        while p and #mcache.path < 50 do
            mcache.path = p.displayname .. '\u{00bb} ' .. mcache.path
            p = p:parent()
        end

        if p then mcache.path = '...\u{00bb} ' .. mcache.path end

        M.tooltipwin.cache[markerpack][typeid][markerid] = mcache

        marker = mcache
    end

    self.box:removeitem(self.descriptiontxt)

    self.titletxt:text(marker.title)
    self.pathtxt:text(marker.path)

    if marker.description then
        self.descriptiontxt:text(marker.description)
        self.box:insertafter(self.pathtxt, self.descriptiontxt, 'start', false)
    end

    for i, field in pairs(self.fields) do
        local name = field[1]
        self.fieldtxt[name]:text(marker[name])
    end
end

function M.addcategorymarkers(category)
    local packpath = category.markerpack.path
    local typeid = category.typeid

    local markercount = 0
    local b6id = string.format('%d|%d', ml.context.mapid(), ml.context.shardid())

    local defaultcolor = ms:get('defaultMarkerColor')
    local defaultalpha = ms:get('defaultMarkerAlpha')
    local defaultheightoffset = ms:get('defaultHeightOffset')
    local defaulticonsize = ms:get('defaultIconSize')
    local markerbasesize = ms:get('markerBaseSize')
    local defaultmapsize = ms:get('defaultMarkerMapDisplaySize')

    for m in category:markersinmapiter(ml.context.mapid()) do
        if m.guid ~= nil then
            if m.behavior == 1 and M.behaviormgr.mapguids[m.guid] then
                goto nextmarker
            elseif m.behavior == 2 and md.guidactive(m.guid, 'day') then
                goto nextmarker
            elseif m.behavior == 3 and md.guidactive(m.guid, 'permanent') then
                goto nextmarker
            elseif m.behavior == 6 and md.guidactive(m.guid, 'day', b6id) then
                goto nextmarker
            elseif m.behavior == 7 and md.guidactive(m.guid, 'day', ml.identity.name()) then
                goto nextmarker
            elseif m.behavior == 101 and md.guidactive(m.guid, 'week') then
                goto nextmarker
            end
        end

        local color = ((m.color or defaultcolor) << 8) | 0xFF
        local alpha_f = m.alpha or defaultalpha
        local alpha = math.tointeger(math.floor(alpha_f * 255))
        color = (color & 0xFFFFFF00) | alpha

        local worldmarkerattrs = {
            x = m2in(m.xpos),
            y = m2in(m.ypos + (m.heightoffset or defaultheightoffset)),
            z = m2in(m.zpos),
            size = (m.iconsize or defaulticonsize) * markerbasesize,
            fadefar = m.fadefar or -1,
            fadenear = m.fadenear or -1,
            color = color,
            mousetest = ((m.minimapvisibility or 1)==1 or (m.mapvisibility or 1)==1),
            tags = {
                pack = packpath,
                typeid = typeid,
                markerid = m.id,
                guid = m.guid or '',
            }
        }

        local cx, cy = M.coordconv:map2continent(worldmarkerattrs.x, worldmarkerattrs.z)

        local mapattrs = {
            x = cx,
            y = cy,
            z = 0.0,
            size = m.mapdisplaysize or defaultmapsize,
            tags = worldmarkerattrs.tags,
            mousetest = true,
        }

        local texturename = m.iconfile

        if not texturename then
            overlay.logerror(string.format("No IconFile for %s", typeid))
            texturename = 'default_texture.png'
        end

        if not M.textures:has(texturename) then
            local texture = category.markerpack:datafile(texturename)
            local texdata = texture:data()

            if not texdata then
                overlay.logerror(string.format("Missing marker image: %s", texturename))
                local default = io.open('textures/eg-overlay-256x256.png', 'rb')
                texdata = default:read('a')
                default:close()
            end

            M.textures:add(texturename, texdata)
        end

        if (m.ingamevisibility or 1) == 1 then
            M.worldsprites:add(texturename, worldmarkerattrs)

            M.behaviormgr:addmarker(m)
        end

        if (m.minimapvisibility or 1) == 1 or (m.mapvisibility or 1) == 1 then
            M.mapsprites:add(texturename, mapattrs)
        end

        markercount = markercount + 1

        ::nextmarker::
    end

    overlay.logdebug(string.format('Loaded %d markers from %s.', markercount, category.typeid))
end

function M.addcategorytrails(category)
    local packpath = category.markerpack.path
    local typeid = category.typeid

    local trailcount = 0

    for trail in category:trailsinmapiter(ml.context.mapid()) do
        local segments = {}

        local coords = {}

        for x,y,z in trail:pointsiter() do
            if x==0.0 and y==0.0 and z==0.0 then
                -- a special case that some trails use to separate segments
                -- some trails have little 1 coordinate segments, which I assume
                -- must be erroneous and are probably ignored by TaCO and Blish
                -- ...so we'll ignore them to
                if #coords > 1 then table.insert(segments, coords) end
                coords = {}
            else
                table.insert(coords, { m2in(x), m2in(y), m2in(z) })
            end
        end
        if #coords > 1 then table.insert(segments, coords) end

        local texturename = trail.texture

        local color = ((trail.color or 0xFFFFFF) << 8) | 0xFF
        local alpha = math.tointeger((trail.alpha or 1.0) * 255.0) or 255
        color = (color & 0xFFFFFF00) | alpha

        if not M.textures:has(texturename) then
            local texture = category.markerpack:datafile(texturename)
            local texdata = texture:data()

            M.textures:add(texturename, texdata)
        end

        if (trail.ingamevisibility or 1) == 1 then
            for i, c in ipairs(segments) do
                local attrs = {
                    fadefar = trail.fadefar or -1,
                    fadenear = trail.fadenear or -1,
                    color = color,
                    wall = (trail.iswall or 0)==1,
                    size = (trail.trailscale or 1.0) * 40,
                    points = c,
                    tags = {
                        pack = packpath,
                        typeid = typeid,
                        trailid = trail.id,
                    },
                }
                M.worldtrails:add(texturename, attrs)
            end
        end

        if (trail.minimapvisibility or 1)==1 or (trail.mapvisibility or 1)==1 then
            for i, coords in ipairs(segments) do
                local contcoords = {}

                for i,c in ipairs(coords) do
                    local cx, cy = M.coordconv:map2continent(c[1], c[3])
                    table.insert(contcoords, {cx, cy, 0.5})
                end

                local attrs = {
                    color = color,
                    wall = false,
                    size = (trail.trailscale or 1.0) * 20,
                    points = contcoords,
                    tags = {
                        pack = packpath,
                        typeid = typeid,
                        trailid = trail.id,
                    },
                }

                M.maptrails:add(texturename, attrs)
            end
        end

        trailcount = trailcount + 1
    end

    overlay.logdebug(string.format('Loaded %d trails from %s.', trailcount, category.typeid))
end

function M.loadmarkerpack(path)
    M.markerpacks[path] = mp.markerpack:new(path)
    M.behaviormgr.mapguids = {}
    M.reloadcategories(true)
    overlay.queueevent('markers-packs-updated')
end

function M.unloadmarkerpack(path)
    M.markerpacks[path] = nil
    M.behaviormgr.mapguids = {}
    M.reloadcategories(true)
    overlay.queueevent('markers-packs-updated')
end

function M.showcategory(category)
        local packpath = category.markerpack.path
        local typeid = category.typeid

        if not M.activetypeids[packpath] then M.activetypeids[packpath] = {} end

        -- this typeid is already shown
        if M.activetypeids[packpath][typeid] then return end

        M.addcategorymarkers(category)
        M.addcategorytrails(category)

        M.activetypeids[packpath][typeid] = true
end

function M.hidecategory(category)
    local packpath = category.markerpack.path
    local typeid = category.typeid

    local tags = {pack = packpath, typeid = typeid}

    M.worldsprites:remove(tags)
    M.mapsprites:remove(tags)
    M.worldtrails:remove(tags)
    M.maptrails:remove(tags)

    for child in category:childreniter() do
        M.hidecategory(child)
    end

    M.behaviormgr:removecategory(typeid)

    M.activetypeids[packpath][typeid] = nil
end

function M.reloadcategories(clear)
    if clear then
        M.activetypeids = {}

        M.behaviormgr:clear()

        M.textures:clear()
        M.worldsprites:clear()
        M.mapsprites:clear()
        M.worldtrails:clear()
        M.maptrails:clear()
    else
        for packpath, typeids in pairs(M.activetypeids) do
            local pack = M.markerpacks[packpath]

            for typeid,_ in pairs(typeids) do
                local cat = pack:category(typeid, false)

                if not md.iscategoryactive(cat, true) then
                    M.hidecategory(cat)
                end
            end
        end
    end

    if M.coordconv==nil then
        overlay.logerror("Invalid coordinate converter for this map, can't display markers.")
        return
    end

    for packpath, pack in pairs(M.markerpacks) do
        for cat in pack:categoriesinmapiter(ml.context.mapid()) do
            if md.iscategoryactive(cat, true) then
                M.showcategory(cat)
            end
            coroutine.yield()
        end
    end
end

function M.onstartup(event, data)
    local packpaths = ms:get('markerpacks') or {}

    overlay.loginfo('Loading marker packs...')

    for i,packpath in ipairs(packpaths) do
        M.markerpacks[packpath] = mp.markerpack:open(packpath)
    end

    if ml.context.mapid() ~= 0 then
        M.onmapchanged()
    end
end

function M.onmapchanged(event, data)
    M.coordconv = gw2.coordinateconverter.new()
    M.behaviormgr.mapguids = {}
    M.reloadcategories(true)
end

function M.onmlavailable(event, data)
    if ms:get('showMarkers') then
        M.worldsprites:draw(true)
        M.mapsprites:draw(true)
        M.worldtrails:draw(true)
        M.maptrails:draw(true)
    end
end

function M.onmlunavailable(event, data)
    M.worldsprites:draw(false)
    M.mapsprites:draw(false)
    M.worldtrails:draw(false)
    M.maptrails:draw(false)
end

function M.setshowmarkers(show)
    show = show == true
    ms:set('showMarkers', show)
    M.worldsprites:draw(show)
    M.mapsprites:draw(show)
    M.worldtrails:draw(show)
    M.maptrails:draw(show)
end

function M.onupdate()
    local worldhovertags = M.worldsprites:mousehovertags()
    local maphovertags = M.mapsprites:mousehovertags()

    local worldcount = #worldhovertags
    local mapcount = #maphovertags

    if mapcount > 0 then
        local tags = maphovertags[mapcount]
        M.tooltipwin:setmarker(tags.pack, tags.typeid, tags.markerid)
        M.tooltipwin:show()
    elseif worldcount > 0 then
        local tags = worldhovertags[worldcount]
        M.tooltipwin:setmarker(tags.pack, tags.typeid, tags.markerid)
        M.tooltipwin:show()
    else
        M.tooltipwin:hide()
    end

    M.behaviormgr:update()
end

overlay.addeventhandler('startup'                , M.onstartup)
overlay.addeventhandler('update'                 , M.onupdate)
overlay.addeventhandler('mumble-link-map-changed', M.onmapchanged)
overlay.addeventhandler('mumble-link-available'  , M.onmlavailable)
overlay.addeventhandler('mumble-link-unavailable', M.onmlunavailable)

overlay.addkeybindhandler('f', function()
    M.behaviormgr:activatemarker()

    return false
end)

return M
