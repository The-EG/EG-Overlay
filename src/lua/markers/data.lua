local logger = require 'logger'
local overlay = require 'eg-overlay'
local sqlite = require 'sqlite'

local data = {}

data.log = logger.logger:new('markers.data')

local markerpack_create_sql = [[
CREATE TABLE markerpack (
    id INTEGER PRIMARY KEY,
    type TEXT NOT NULL,
    path TEXT NOT NULL
)
]]

local category_create_sql = [[
CREATE TABLE category (
    typeid TEXT PRIMARY KEY NOT NULL,
    parent TEXT,
    active BOOL NOT NULL DEFAULT TRUE,
    FOREIGN KEY (parent) REFERENCES category (typeid)
)
]]

local category_props_create_sql = [[
CREATE TABLE category_props (
    id INTEGER PRIMARY KEY,
    category TEXT NOT NULL,
    property TEXT NOT NULL,
    value,
    FOREIGN KEY (category) REFERENCES category (typeid) ON DELETE CASCADE,
    UNIQUE (category, property)
)
]]

local poi_create_sql = [[
CREATE TABLE poi (
    id INTEGER PRIMARY KEY,
    type TEXT NOT NULL,
    markerpack INTEGER NOT NULL,
    FOREIGN KEY (type) REFERENCES category (typeid) ON DELETE CASCADE,
    FOREIGN KEY (markerpack) REFERENCES markerpack (id) ON DELETE CASCADE
)
]]

local poi_props_create_sql = [[
CREATE TABLE poi_props (
    id INTEGER PRIMARY KEY,
    poi INTEGER NOT NULL,
    property TEXT NOT NULL,
    value,
    FOREIGN KEY (poi) REFERENCES poi (id) ON DELETE CASCADE,
    UNIQUE (poi, property)
)
]]

local trail_create_sql = [[
CREATE TABLE trail
(
    id INTEGER PRIMARY KEY,
    type TEXT NOT NULL REFERENCES category (typeid) ON DELETE CASCADE,
    markerpack INTEGER_NOT NULL REFERENCES markerpack (id) ON DELETE CASCADE
)
]]

local trail_props_create_sql = [[
CREATE TABLE trail_props (
    id INTEGER PRIMARY KEY,
    trail INTEGER NOT NULL REFERENCES trail (id) ON DELETE CASCADE,
    property TEXT NOT NULL,
    value,
    UNIQUE (trail, property)
)
]]

local trail_coords_create_sql = [[
CREATE TABLE trail_coord (
    id INTEGER PRIMARY KEY,
    seq INTEGER NOT NULL,
    trail INTEGER NOT NULL REFERENCES trail (id) ON DELETE CASCADE,
    x REAL NOT NULL,
    y REAL NOT NULL,
    UNIQUE (trail, seq)
)
]]

data.db = sqlite.open(overlay.data_folder('markers') .. 'markers.db')

local function tableexists(name)
    return data.db:execute(string.format('PRAGMA table_list(%s)', name))
end

local function verifydatabase()
    if not tableexists('markerpack')     then data.db:execute(markerpack_create_sql) end
    if not tableexists('category')       then data.db:execute(category_create_sql) end
    if not tableexists('category_props') then data.db:execute(category_props_create_sql) end
    if not tableexists('poi')            then data.db:execute(poi_create_sql) end 
    if not tableexists('poi_props')      then data.db:execute(poi_props_create_sql) end
    if not tableexists('trail')          then data.db:execute(trail_create_sql) end
    if not tableexists('trail_props')    then data.db:execute(trail_props_create_sql) end
    if not tableexists('trail_coord')    then data.db:execute(trail_coords_create_sql) end
end

verifydatabase()

local markerpack = {}
markerpack.__index = markerpack

function markerpack:new(path, packtype)
    if packtype~='zip' and packtype~='folder' then
        error('markerpack:new(path, packtype) - packtype must be either "zip" or "folder".',1)
    end

    local mp = {}

    local s = data.db:prepare('SELECT id, type, path FROM markerpack WHERE path = ?')
    s:bind(1, path)

    local r = s:step()
    s:finalize()

    if not r then
        s = data.db:prepare('INSERT INTO markerpack (path, type) VALUES (?, ?) RETURNING id')
        s:bind(1, path)
        s:bind(2, packtype)
        local ir = s:step()
        s:finalize()
        mp.id = ir.id
    else
        if r['type'] ~= packtype then
            error('markerpack type mismtach.', 1)
        end
        mp.id = r.id
    end

    setmetatable(mp, self)

    return mp
end

function markerpack:addpoi(props)
    if not props['type'] then
        error("POI properties must include a 'type'.", 1)
    end

    local poi_insert = data.db:prepare('INSERT INTO poiq (type, markerpack) VALUES (?, ?) RETURNING id')
    local poi_prop_insert = data.db:prepare('INSERT INTO poi_props (poi, property, value) VALUES (?, lower(?), ?)')

    poi_insert:bind(1, props['type'])
    poi_insert:bind(2, self.id)
    local poir = poi_insert:step()
    poi_insert:finalize()

    local poi_id = poir.id

    for k, v in pairs(props) do
        if k~='type' then
            poi_prop_insert:bind(1, poi_id)
            poi_prop_insert:bind(2, k)
            poi_prop_insert:bind(3, v)
            poi_prop_insert:step()
            poi_prop_insert:reset()
        end
    end
    poi_prop_insert:finalize()
end

function markerpack:addtrail(props, coords)
    
end

function markerpack:delete()
    local s = data.db:prepare('DELETE FROM markerpack WHERE id = ?')
    s:bind(1, self.id)
    s:step()
    s:finalize()
end

local category = {}
category.__index = category

function category:new(typeid, active)
    local cat = { typeid = typeid }

    local s = data.db:prepare('SELECT typeid FROM category WHERE typeid = ?')
    s:bind(1, typeid)
    r = s:step()
    s:finalize()

    if not r then
        local parentids = {}
        for t in string.gmatch(typeid, '[^%.]+') do
            table.insert(parentids, t)
        end
        table.remove(parentids) -- remove the last name

        local parent = table.concat(parentids,'.')
        if parent=='' then parent = nil end

        s = data.db:prepare('INSERT INTO category (typeid, parent, active) VALUES (?,?,?)')
        s:bind(1, typeid)
        s:bind(2, parent)
        s:bind(3, active)
        s:step()
        s:finalize()
    end

    setmetatable(cat, self)
    return cat
end

function category:prop(name, value)
    if value==nil then
        local s = data.db:prepare('SELECT value FROM category_props WHERE category = ? and property = lower(?)')
        s:bind(1, self.typeid)
        s:bind(2, name)
        r = s:step()
        s:finalize()
        if r then return r.value end
    else
        local s = data.db:prepare('INSERT INTO category_props (value, category, property) VALUES (:value , :typeid , lower(:property) ) ON CONFLICT DO UPDATE SET value = :value WHERE category = :typeid and property = lower(:property)')
        s:bind(':value', value)
        s:bind(':typeid', self.typeid)
        s:bind(':property', name)
        s:step()
        s:finalize()
    end
end

function category:active(value)
    if value==nil then
        local s = data.db:prepare('SELECT active FROM category WHERE typeid = ?')
        s:bind(1, value)
        local r = s:step()
        s:finalize()
        if not r then
            error("Category " .. self.typeid .. " doesn't exist!", 1)
        end
        return r.active
    else
        local s = data.db:prepare('UPDATE category SET active = ? WHERE typeid = ?')
        s:bind(1, active)
        s:bind(2, self.typeid)
        s:step()
        s:finalize()
    end
end

-- function category:__index(key)
--     return self:prop(key)
-- end

-- function category:__newindex(key, value)
--     self:prop(key, value)
-- end

function data.markerpack(path, packtype)
    return markerpack:new(path, packtype)
end

function data.category(typeid, active)
    return category:new(typeid, active)
end

function data.categories()
    local s = data.db:prepare("SELECT typeid FROM category WHERE typeid NOT LIKE '%.%'")
    return function()
        local r = s:step()
        if not r then
            s:finalize()
        else
            return data.category(r.typeid)
        end
    end
end

return data