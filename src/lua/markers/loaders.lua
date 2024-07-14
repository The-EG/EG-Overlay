local zip = require 'zip'
local xml = require 'libxml2'
local logger = require 'logger'
local overlay = require 'eg-overlay'
local converters = require 'markers.converters'
local data = require 'markers.data'

local loaders = {}

loaders.log = logger.logger:new('markers.loaders')

local function node_error(node, ...)
    local errdoc = node:doc():url()
    local errline = node:line()
    local message = string.format(...)
    loaders.log:error(string.format("%s:%d: %s", errdoc, errline, message))
end

local function node_warning(node, ...)
    local errdoc = node:doc():url()
    local errline = node:line()
    local message = string.format(...)
    loaders.log:warn(string.format('%s:%d: %s', errdoc, errline, message))
end

local function validate_poi_props(node)
    local props = {}
    for i,p in ipairs(node:props()) do
        local plower = string.lower(p)

        if converters.fromxml[plower] then
            props[plower] = converters.fromxml[plower](node:prop(p))
            if not props[plower] then
                node_error(node, "Couldn't convert value %s for property %s", node:prop(p), p)
            end
        elseif string.match(p, 'bh%-.*') then
            -- Blish Hud specific property override, ignored for now
        else
            node_warning(node, "unknown attribute: %s", p)
        end
    end
    return props
end

-- get child elements of a particular type/tag
local function get_child_elements_by_tag(node, tag)
    local children = {}

    local tag_lower = string.lower(tag)
    
    local child = node:children()

    while child ~= nil do
        if child:type() == 'element-node' and string.lower(child:name()) == tag_lower then
            table.insert(children, child)
        end
        child = child:next()
    end

    return children
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

function base_loader:load_xml_doc(doc)
    local root = doc:get_root_element()

    if string.lower(root:name()) ~= "overlaydata" then
        node_error(root, "Root element isn't OverlayData, found %s", root:name())
        return
    end

    local marker_cats = get_child_elements_by_tag(root, "MarkerCategory")
    for i, cat in ipairs(marker_cats) do
        self:load_marker_category(cat)
    end

    local pois_container = get_child_elements_by_tag(root, "POIs")[1]
    if pois_container then
        local pois = get_child_elements_by_tag(pois_container, "POI")
        for i, poi in ipairs(pois) do
            self:load_poi(poi)
        end

        local trails = get_child_elements_by_tag(pois_container, "Trail")
        for i, trail in ipairs(trails) do
            --self:load_trail(trail)
            --if coroutine.isyieldable() then coroutine.yield() end
        end
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

    table.insert(self.category_typeids, name)
    local child_cats = get_child_elements_by_tag(node, "MarkerCategory")
    for i, cat in ipairs(child_cats) do
        self:load_marker_category(cat)
    end
    table.remove(self.category_typeids)
end

function base_loader:load_poi(node)
    local poi_props = validate_poi_props(node)

    if not poi_props.mapid then
        node_error(node, "POI element must have a MapID attribute. Ignoring POI.")
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

    self.db_mp:addpoi(poi_props)
end

function base_loader:load_trail(node)
    local trail_props = validate_poi_props(node)

    if not trail_props.traildata then
        node_error(node, "Trail element must have a trailData attribute, ignoring Trail.")
        return
    end

    local trail = data.trail:new()
    
    for p,v in pairs(trail_props) do
        trail.properties[p] = v
    end
    
    local trail_file = self.zip:file_content(trail.properties.traildata)

    if trail_file == nil then
        node_warning(node, "trailData file (%s) does not exist, ignoring Trail.", trail.properties.traildata)
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

    trail.properties.mapid = map_id

    -- local trail_file_pos = 9
    -- repeat
    --     local x, y, z, l = string.unpack('<fff', trail_file, trail_file_pos)
    --     trail_file_pos = l
    --     --if not l then break end

    --     table.insert(trail.points, {
    --         x = x,
    --         y = y,
    --         z = z
    --     })
    -- until trail_file_pos > trail_file_len

    table.insert(self.trails, trail)
end

function base_loader:get_category_for_type(typeid)
    for i,c in ipairs(self.categories) do
        local cat = c:get_category_for_type(typeid)
        if cat then return cat end
    end
end

loaders.zip_loader = {}
loaders.zip_loader.__index = loaders.zip_loader
setmetatable(loaders.zip_loader, base_loader)

function loaders.zip_loader:new(zip_path)
    local z = base_loader:new()
    z.zip_path = zip_path
    z.db_mp = data.markerpack(zip_path, 'zip')
    z.category_typeids = {}
    z.pois = {}
    z.trails = {}
    setmetatable(z, self)
    return z
end

function loaders.zip_loader:load()
    loaders.log:info("Loading %s...", self.zip_path)
    self.zip = zip.open(self.zip_path)

    local files = self.zip:files()
    local start = overlay.time()

    data.db:execute('BEGIN TRANSACTION')

    for i,f in ipairs(files) do
        -- find all top level xml files
        if string.find(f, '[^/]+%.xml') == 1 then
            local fstr = self.zip:file_content(f)
            local xml = xml.read_string(fstr, f)
            self:load_xml_doc(xml)
            if coroutine.isyieldable() then coroutine.yield() end
        end
    end

    data.db:execute('COMMIT TRANSACTION')

    local endt = overlay.time()
    local dur = endt - start
    self.zip = nil
    loaders.log:info(string.format("Loaded %s in %.3f seconds, %d pois, %d trails", self.zip_path, dur, #self.pois, #self.trails))
end

return loaders