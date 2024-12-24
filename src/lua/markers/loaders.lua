local zip = require 'zip'
local xml = require 'libxml2'
local logger = require 'logger'
local overlay = require 'eg-overlay'
local converters = require 'markers.converters'
local mp = require 'markers.package'
local xmlcleaner = require 'xml-cleaner'

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

function base_loader:loaddatafile(path)
    local df = self.mp:datafile(path)

    if df:data()==nil then
        local data = self:filecontent(path)
        if not data then
            return false
        else
            df:data(data)
        end
    end

    return true
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

    -- if props.defaulttoggle~=0 then
    --     cat:active(true)
    -- end

    if props.iconfile then
        if not self:loaddatafile(props.iconfile) then
            xmlerror(file, line, "iconfile %s does not exist", props.iconfile)
        end
    end

    if props.texture then
        if not self:loaddatafile(props.texture) then
            xmlerror(file, line, "texture %s does not exist", props.texture)
        end
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

    -- if attrs.xpos==nil or attrs.ypos==nil or attrs.zpos==nil then
    --     xmlerror(file, line, 'POI with invalid location, ignoring.')
    --     return
    -- end

    local props = validatemarkerprops(file, line, attrs)

    local typeid = attrs.type

    local cat = self.mp:category(typeid, true)
    local marker = cat:newmarker()

    if props.mapid==nil then
        xmlwarn(file, line, 'POI with no MapID.')
    else
        marker:mapid(props.mapid)
    end

    for p, v in pairs(props) do
        marker[p] = v
    end

    if props.iconfile then
        if not self:loaddatafile(props.iconfile) then
            xmlerror(file, line, "iconfile %s does not exist", props.iconfile)
        end
    end
end

function base_loader.begintag.Trail(self, file, line, attrs)
    if attrs.type==nil then
        xmlerror(file, line, 'Trail with no type, ignoring.')
        return
    end
  
    local props = validatemarkerprops(file, line, attrs)

    if not props.traildata then
        xmlerror(file, line, 'Trail with no trailData, ignoring.')
        return
    end

    local typeid = attrs.type
    local cat = self.mp:category(typeid, true)
    local trail = cat:newtrail()

    for p,v in pairs(props) do
        trail[p] = v
    end


    local trail_file = self:filecontent(props.traildata)

    if trail_file == nil then
        xmlerror(file, line, "trailData file (%s) does not exist.", props.traildata)
        return
    end

    local trail_file_len = #trail_file
    if trail_file_len < 8 + 12 then
        xmlerror(file, line, "Trail file too short.")
        return
    end

    local trailver, map_id = string.unpack('<I4I4', trail_file)

    if trailver~=0 then
        xmlerror(file, line, "Trail data file unrecognized format.")
        return
    end

    if not props.mapid then trail.mapid = map_id end

    trail:mapid(map_id)

    local trail_file_pos = 9
    repeat
        local x, y, z, l = string.unpack('<fff', trail_file, trail_file_pos)
        trail_file_pos = l
        --if not l then break end
        
        trail:addpoint(x, y, z)

    until trail_file_pos > trail_file_len

    if props.texture then
        if not self:loaddatafile(props.texture) then
            xmlerror(file, line, "texture %s does not exist", props.texture)
        end
    end
end

function base_loader:loadxml(name, xmldata)
    local cleanxml = xmlcleaner.cleanxml(xmldata, name)
    --local cleanxml = xmldata
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

    xml.read_string(cleanxml, name, {
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
    z.pack_path = pack_path
    z.mp = mp.markerpack:new(pack_path)
    z.categorystack = {}

    setmetatable(z, self)
    return z
end

function loaders.zip_loader:filecontent(path)
    return self.zip:file_content(path)
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
    self.mp.db:execute('PRAGMA cache_size = 100000')
    self.mp.db:execute('PRAGMA foreign_keys = OFF')
    --self.mp.db:execute('BEGIN IMMEDIATE TRANSACTION')

    for i,f in ipairs(files) do
        -- find all top level xml files
        if string.find(f, '[^/]+%.xml') == 1 then
            local fstr = self.zip:file_content(f)
            self.mp.db:execute('BEGIN TRANSACTION')
            self:loadxml(f, fstr)
            self.mp.db:execute('COMMIT TRANSACTION')
            coroutine.yield()
        end
    end

    --self.mp.db:execute('COMMIT TRANSACTION')
    self.mp.db:execute('PRAGMA foreign_keys = ON')
    
    -- loaders.log:info("Checking data integrity...")
    -- local stmt = self.mp.db:prepare('PRAGMA foreign_key_check(markers)')
    -- local function marker_rows()
    --     return stmt:step()
    -- end

    -- local badtypeids = {}
    -- local badcount = 0
    -- for row in marker_rows do
    --     local ps = self.mp.db:prepare('SELECT id, type FROM markers WHERE rowid = ?')
    --     ps:bind(1, row.rowid)
    --     local marker = ps:step()
    --     badtypeids[marker.type] = true
    --     badcount = badcount + 1
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
    local catcount = self.mp.db:execute([[SELECT COUNT(*) AS count FROM categories]]).count
    local poicount = self.mp.db:execute([[SELECT COUNT(*) AS count FROM markers]]).count
    local trailcount = self.mp.db:execute([[SELECT COUNT(*) AS count FROM trails]]).count
    local trailpoints = self.mp.db:execute([[SELECT COUNT(*) AS count FROM trailcoords]]).count
    local datacount = self.mp.db:execute([[SELECT COUNT(*) AS count FROM datafiles]]).count
    self.zip = nil

    loaders.log:info("Converted %s in %.3f seconds:", self.zip_path, dur)
    loaders.log:info("  %d categories", catcount)
    loaders.log:info("  %d POIs", poicount)
    loaders.log:info("  %d trails", trailcount)
    loaders.log:info("  %d trail points", trailpoints)
    loaders.log:info("  %d data files", datacount)

    loaders.log:info("Optimizing database...")
    self.mp.db:execute('PRAGMA optimize')
    loaders.log:info("Vacuuming...")
    self.mp.db:execute('VACUUM')
end

return loaders
