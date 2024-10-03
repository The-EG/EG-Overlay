local zip = require 'zip'
local xml = require 'libxml2'
local logger = require 'logger'
local overlay = require 'eg-overlay'
local converters = require 'markers.converters'
local mp = require 'markers.package'

local loaders = {}

loaders.log = logger.logger:new('markers.loaders')

local function xmlerror(file, line, ...)
    local msg = string.format(...)
    loaders.log:error("%s:%d: %s", file, line, msg)
end

local function xmlwarn(file, line, ...)
    local msg = string.format(...)
    loaders.log:warn("%s:%d: %s", file, line, msg)
end

local function validatemarkerprops(file, line, attrs)
    local props = {}

    for k,v in pairs(attrs) do
        local plower = string.lower(k)

        if converters.fromxml[plower] then
            props[plower] = converters.fromxml[plower](v)
            if not props[plower] then
                xmlerror(file, line, "Couldn't convert value %s for property %s", v, k)
            end
        elseif string.match(k, 'bh%-.*') then
            -- Blish Hud specific property override, ignored for now
        else
            xmlwarn(file, line, "Unknown attribute: %s", k)
        end
    end
    return props
end

local base_loader = {}
base_loader.__index = base_loader

function base_loader:new()
    local b = {
        categories = {},
        pois = {},
        trails = {}
    }
    setmetatable(b, self)
    return b
end

-- a table of functions to handle begin/end tags
base_loader.begintag = {}
base_loader.endtag = {}

function base_loader.begintag.OverlayData(self, file, line, attrs)
    self.in_overlaydata = true
end

function base_loader.endtag.OverlayData(self)
    self.in_overlaydata = false
end

function base_loader.begintag.MarkerCategory(self, file, line, attrs)
    if not self.in_overlaydata then
        xmlerror(file, line, 'Got MarkerCategory outside of OverlayData.')
        return
    end

    local name = attrs.name or attrs.Name or attrs['bh-name']

    if not name then
        xmlerror(file, line, 'MarkerCategory must have a name attribute.')
        return
    end

    local props = validatemarkerprops(file, line, attrs)
    
    table.insert(self.categorystack, name)
    local typeid = table.concat(self.categorystack, '.')

    local cat = self.mp:category(typeid, true)

    for p, v in pairs(props) do
        cat[p] = v
    end
end

function base_loader.endtag.MarkerCategory(self)
    table.remove(self.categorystack)
end

function base_loader.begintag.POIs(self, file, line, attrs)
    if not self.in_overlaydata then
        xmlerror(file, line, 'Got POIs outside of OverlayData.')
        return
    end

    self.in_pois = true
end

function base_loader.endtag.POIs(self)
    self.in_pois = false
end

function base_loader.begintag.POI(self, file, line, attrs)
    if attrs.type==nil then
        xmlerror(file, line, 'POI with no type, ignoring.')
        return
    end

    if attrs.xpos==nil or attrs.ypos==nil or attrs.zpos==nil then
        xmlerror(file, line, 'POI with invalid location, ignoring.')
        return
    end

    if attrs.MapID==nil then
        xmlwarn(file, line, 'POI with no MapID.')
    end

    local props = validatemarkerprops(file, line, attrs)

    local typeid = attrs.type

    local cat = self.mp:category(typeid)
    local poi = cat:newpoi()

    for p, v in pairs(props) do
        poi[p] = v
    end
end

function base_loader:load_marker_category(node)
    -- name can be a few different things...
    local name = node:prop("name")
    name = name or node:prop("Name")
    name = name or node:prop("bh-name")

    if not name then
        node_error(node, "MarkerCategory must have a name attribute.")
        return
    end

    local cat_props = validate_poi_props(node)
    local parentname = table.concat(self.category_typeids,'.')

    local typeid = name
    if parentname~='' then typeid = parentname .. '.' .. typeid end
    local defactive = cat_props.defaulttoggle or cat_props.toggledefault or 1

    local category = data.category(typeid, defactive)
    for p,v in pairs(cat_props) do
        category:prop(p, v)
    end

    if cat_props.iconfile then
        local icon = data.datafile(cat_props.iconfile)

        if not icon then
            local icondata = self.zip:file_content(cat_props.iconfile)
            if not icondata then
                node_error(node, "iconFile %s does not exist.", cat_props.iconfile)
                return
            end

            icon = data.datafile(cat_props.iconfile, icondata)
        end
    end

    table.insert(self.category_typeids, name)
    local child_cats = get_child_elements_by_tag(node, "MarkerCategory")
    for i, cat in ipairs(child_cats) do
        self:load_marker_category(cat)
    end
    table.remove(self.category_typeids)
end

function base_loader:load_poi(node)
    local poi_props = validate_poi_props(node)

    if not poi_props.mapid and not poi_props.type then
        node_error(node, "POI element must have a MapID or type attribute. Ignoring POI.")
        return
    end

    if not poi_props.xpos then
        node_error(node, "POI element must have a xpos attribute. Ignoring POI.")
        return
    end
    if not poi_props.ypos then
        node_error(node, "POI element must have a ypos attribute. Ignoring POI.")
        return
    end
    if not poi_props.zpos then
        node_error(node, "POI element must have a zpos attribute. Ignoring POI.")
        return
    end

    local r, err = pcall(self.db_mp.addpoi, self.db_mp, poi_props)
    if not r then
        node_error(node, "Failed to insert POI: %s", err)
    end

    if poi_props.iconfile then
        local icon = data.datafile(poi_props.iconfile)

        if not icon then
            local icondata = self.zip:file_content(poi_props.iconfile)
            if not icondata then
                node_error(node, "iconFile %s does not exist.", poi_props.iconfile)
                return
            end

            icon = data.datafile(poi_props.iconfile, icondata)
        end
    end
end

function base_loader:load_trail(node)
    local trail_props = validate_poi_props(node)

    if not trail_props.traildata then
        node_error(node, "Trail element must have a trailData attribute, ignoring Trail.")
        return
    end

    if not trail_props['type'] then
        node_error(node, "Trail element must have a type attribute, ignoring Trail.")
        return
    end
    
    local trail_file = self.zip:file_content(trail_props.traildata)

    if trail_file == nil then
        node_warning(node, "trailData file (%s) does not exist, ignoring Trail.", trail_props.traildata)
        return
    end

    local trail_file_len = #trail_file
    if trail_file_len < 8 + 12 then
        node_error(node, "Trail file too short.")
        return
    end

    local trailver, map_id = string.unpack('<I4I4', trail_file)

    if trailver~=0 then
        node_error(node, "Trail data file unrecognized format.")
        return
    end

    if not trail_props.map_id then trail_props.mapid = map_id end

    local trail_coords = {}

    local trail_file_pos = 9
    repeat
        local x, y, z, l = string.unpack('<fff', trail_file, trail_file_pos)
        trail_file_pos = l
        --if not l then break end

        table.insert(trail_coords, { x, y, z })
    until trail_file_pos > trail_file_len

    self.db_mp:addtrail(trail_props, trail_coords)
end

function base_loader:loadxml(name, xmldata)
    local function startele(name, attrs, file, line)
        if self.begintag[name] then
            self.begintag[name](self, file, line, attrs)
        else
            --xmlwarn(file, line, 'Unhandled tag: %s', name)
        end
    end

    local function endele(name)
        if self.endtag[name] then
            self.endtag[name](self)
        end
    end

    xml.read_string(xmldata, name, {
        startelement = startele,
        endelement = endele
    })
end

loaders.zip_loader = {}
loaders.zip_loader.__index = loaders.zip_loader
setmetatable(loaders.zip_loader, base_loader)

function loaders.zip_loader:new(zip_path, pack_path)
    local z = base_loader:new()
    z.zip_path = zip_path
    z.mp = mp.markerpack:new(pack_path)
    z.categorystack = {}

    setmetatable(z, self)
    return z
end

function loaders.zip_loader:load()
    loaders.log:info("Loading %s...", self.zip_path)
    self.zip = zip.open(self.zip_path)

    local files = self.zip:files()
    local start = overlay.time()

    -- Ideally we could use the database to check the validity of data as we are
    -- inserting it, however marker packs don't necessarily arrange things so 
    -- that categories are created first. Instead, POIs may be inserted before
    -- the categories they belong to.
    -- So, turn off foreign key checks now and then run a check after everything
    -- is loaded.
    self.mp.db:execute('PRAGMA foreign_keys = OFF')
    self.mp.db:execute('BEGIN TRANSACTION')

    for i,f in ipairs(files) do
        -- find all top level xml files
        if string.find(f, '[^/]+%.xml') == 1 then
            local fstr = self.zip:file_content(f)
            loaders.log:info("%s...", f)
            self:loadxml(f, fstr)
            coroutine.yield()
        end
    end

    self.mp.db:execute('COMMIT TRANSACTION')
    self.mp.db:execute('PRAGMA foreign_keys = ON')
    
    -- loaders.log:info("Checking data integrity...")
    -- local stmt = data.db:prepare('PRAGMA foreign_key_check(poi)')
    -- local function poi_rows()
    --     return stmt:step()
    -- end

    -- local badtypeids = {}
    -- local badcount = 0
    -- for row in poi_rows do
    --     local ps = data.db:prepare('SELECT id, type, markerpack FROM poi WHERE rowid = ?')
    --     ps:bind(1, row.rowid)
    --     local poi = ps:step()
    --     if poi.markerpack == self.db_mp.id then
    --         badtypeids[poi.type] = true
    --         badcount = badcount + 1
    --     end
    --     ps:finalize()
    -- end
    -- stmt:finalize()

    -- if badcount > 0 then
    --     loaders.log:error("Bad type ids in %s", self.zip_path)
    --     for k,v in pairs(badtypeids) do
    --         loaders.log:error("  %s", k)
    --     end
    -- end

    -- loaders.log:info("Integrity check complete.")

    local endt = overlay.time()
    local dur = endt - start
    local catcount = self.mp:categorycount()
    self.zip = nil
    loaders.log:info("Loaded %s in %.3f seconds, %d categories",self.zip_path, dur, catcount)
end

return loaders
