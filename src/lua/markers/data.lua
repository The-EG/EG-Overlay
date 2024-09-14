--[[ RST
markers.data
============

.. lua:module:: markers.data


]]--

local logger = require 'logger'
local overlay = require 'eg-overlay'
local sqlite = require 'sqlite'

local data = {}

data.log = logger.logger:new('markers.data')

--[[ RST
Attributes
----------
.. lua:data:: db

    A :lua:class:`sqlite` database housing all imported marker packs.

    .. versionhistory::
        :0.1.0: Added
]]--
data.db = sqlite.open(overlay.data_folder('markers') .. 'markers.db')
data.db:execute('PRAGMA foreign_keys = ON')

local markerpack = {}
markerpack.__index = markerpack

local category = {}
category.__index = category

local datafile = {}
datafile.__index = datafile

--[[ RST
Functions
---------

.. lua:function:: markerpack(path, packtype)

    Create or retrieve a markerpack from the markers database. If the markerpack
    does not exist, it will be created. Returns a new :lua:class:`md_markerpack`.

    :param string path: Path to the markerpack.
    :param string packtype: Type of the pack, either ``'zip'`` for ``'folder'``.
    :rtype: md_markerpack

    .. versionhistory::
        :0.1.0: Added
]]--
function data.markerpack(path, packtype)
    return markerpack:new(path, packtype)
end

--[[ RST
.. lua:function:: category(typeid, active)

    Create or retrieve a category from the markers database. If the category
    does not already exist, it will be created. Returns a new :lua:class:`md_category`.

    :param string typeid: A complete category typeid. This should be the typeid
        of this category and all parents separated by ``.``.
        Ie. ``grandparent.parent.thiscategory``.
    :param boolean active: 
    :rtype: md_category

    .. versionhistory::
        :0.1.0: Added
]]--
function data.category(typeid, active)
    return category:new(typeid, active)
end

--[[ RST
.. lua:function:: datafile(path[, data])

    Create or retrieve a datafile. If ``data`` is omitted and the datafile does
    not exist, ``nil`` is returned.

    :param string path: The datafile path. This is a unique identifier.
    :param string data: (Optional) The binary data.
    :rtype: md_datafile

    .. versionhistory::
        :0.1.0: Added
]]--
function data.datafile(path, filedata)
    return datafile:new(path, filedata)
end

--[[ RST
Classes
-------

.. lua:class:: md_markerpack

    Represents a markerpack or a logical grouping of markers. All markers and
    trails imported from a single marker pack or folder will be grouped under
    the same markerpack.

]]--
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

--[[ RST
    .. lua:method:: addpoi(props)

        Add a POI belonging to this markerpack, with the provided properties.

        :param table props: A table of properties for the POI.

        .. warning::
            ``props`` must include a value for ``type``, which is the category
            id. If ``type`` is not included an error will occur.

        .. versionhistory::
            :0.1.0: Added
]]--
function markerpack:addpoi(props)
    if not props['type'] then
        error("POI properties must include a 'type'.", 1)
    end

    local poi_insert = data.db:prepare('INSERT INTO poi (type, markerpack) VALUES (?, ?) RETURNING id')
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

--[[ RST
    .. lua:method:: addtrail(props, coords)

        Add a POI belonging to this markerpack, with the provided properties and
        coordinates.

        :param table props: A table of properties for the Trail.
        :param sequence coords: A sequence of sequences, ie. ``{ {0.0, 0.0, 0.0}, ...}``.

        .. warning::
            ``props`` must include a value for ``type``, which is the category
            id. If ``type`` is not included an error will occur.

        .. versionhistory::
            :0.1.0: Added
]]--
function markerpack:addtrail(props, coords)
    if not props['type'] then
        error("Trail properties must include a 'type'.", 1)
    end

    local trail_insert = data.db:prepare('INSERT INTO trail (type, markerpack) VALUES (?,?) RETURNING id')
    local trail_prop_insert = data.db:prepare('INSERT INTO trail_props (trail, property, value) VALUES (?, lower(?), ?)')
    local trail_coord_insert = data.db:prepare('INSERT INTO trail_coord (trail, seq, x, y, z) VALUES (?, ?, ?, ?, ?)')

    trail_insert:bind(1, props['type'])
    trail_insert:bind(2, self.id)
    local trailr = trail_insert:step()
    trail_insert:finalize()

    local trail_id = trailr.id

    for k, v in pairs(props) do
        if k~='type' then
            trail_prop_insert:bind(1, trail_id)
            trail_prop_insert:bind(2, k)
            trail_prop_insert:bind(3, v)
            trail_prop_insert:step()
            trail_prop_insert:reset()
        end
    end
    trail_prop_insert:finalize()

    for i, c in ipairs(coords) do
        local x,y,z = table.unpack(c)
        trail_coord_insert:bind(1, trail_id)
        trail_coord_insert:bind(2, i)
        trail_coord_insert:bind(3, x)
        trail_coord_insert:bind(4, y)
        trail_coord_insert:bind(5, z)
        trail_coord_insert:step()
        trail_coord_insert:reset()
    end
    trail_coord_insert:finalize()
end

--[[ RST
    .. lua:method:: delete()

        Delete this marker pack.

        .. danger::
            This will also delete all associated POIs and Trails.

        .. versionhistory::
            :0.1.0: Added
]]--
function markerpack:delete()
    local s = data.db:prepare('DELETE FROM markerpack WHERE id = ?')
    s:bind(1, self.id)
    s:step()
    s:finalize()
end

--[[ RST
.. lua:class:: md_category

    A category is a grouping of POIs and Trails. Categories can be grouped under
    other categories to create a hierarchy categories.
]]--
function category:new(typeid, active)
    local cat = { typeid = typeid }

    local s = data.db:prepare('SELECT typeid FROM category WHERE typeid = ?')
    s:bind(1, typeid)
    local r = s:step()
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

--[[ RST
    .. lua:method:: prop(name[, value])

        Get or set property ``name``.

        If ``value`` is omitted, the current value or ``nil`` is returned.
        Otherwise the property is set to ``value``.

        :param string name:
        :param value: (Optional)
        :rtype: any

        .. versionhistory::
            :0.1.0: Added
]]--
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

--[[ RST
    .. lua:method:: active([value])

        Get or set if this category should be displayed (is active).

        :param boolean value: (Optional)
        :rtype: boolean

        .. versionhistory::
            :0.1.0: Added
]]--
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


--[[ RST
.. lua:class:: md_datafile

    A data file from a marker pack. Generally these are images or textures.

    A data file is identified by its path, which is relative to the root of the
    marker pack. Many marker packs may refer to the same data file path, and
    only the first one that is loaded will be created.
]]--
function datafile:new(path, filedata)
    local df = { path = path }
    local s = data.db:prepare("SELECT path FROM data_file WHERE path = ?")

    s:bind(1, path)
    local r = s:step()
    s:finalize()

    if not r then
        if not filedata then return end

        s = data.db:prepare("INSERT INTO data_file (path, data) VALUES (?, ?)")
        s:bind(1, path)
        s:bind(2, filedata, true)
        s:step()
        s:finalize()
    end

    setmetatable(df, self)

    return df
end

--[[ RST
    .. lua:method:: data()
    
        Return this datafile's binary data.

        :rtype: string

        .. versionhistory::
            :0.1.0: Added
]]--
function datafile:data()
    local s = data.db:prepare("SELECT data FROM data_file WHERE path = ?")
    s:bind(1, self.path)
    local r = s:step()
    s:finalize()

    if not r then
        error(string.format("Data file %s does not exist", self.path), 1)
    end

    return r.data
end


--[[ RST
Database Tables
---------------

.. overlay:database:: markers

.. overlay:dbtable:: markerpack

    Markerpacks

    **Columns**

    =========== ======= ========================================================
    Name        Type    Description
    =========== ======= ========================================================
    id          INTEGER Marker pack id. Internal ID.
    type        TEXT    The type of marker pack, either ``zip`` or ``folder``.
    path        TEXT    The path to the pack or folder.
    =========== ======= ========================================================

    .. versionhistory::
        :0.1.0: Added
]]--
local markerpack_create_sql = [[
CREATE TABLE IF NOT EXISTS markerpack (
    id INTEGER PRIMARY KEY,
    type TEXT NOT NULL,
    path TEXT NOT NULL
)
]]

--[[ RST
.. overlay:dbtable:: category

    Marker/Trail Categories

    **Columns**

    ====== ==== ===========================================================================================
    Name   Type Description
    ====== ==== ===========================================================================================
    typeid TEXT The ID of the category. This is referenced by POIs and Trails that belong to this category.
    parent TEXT The ID of the category this category belongs to (its parent).
    active BOOL Indicates if this category should be displayed or not.
    ====== ==== ===========================================================================================

    .. versionhistory::
        :0.1.0: Added
]]--
local category_create_sql = [[
CREATE TABLE IF NOT EXISTS category (
    typeid TEXT PRIMARY KEY NOT NULL,
    parent TEXT,
    active BOOL NOT NULL DEFAULT FALSE,
    FOREIGN KEY (parent) REFERENCES category (typeid)
)
]]

--[[ RST
.. overlay:dbtable:: category_props

    Category Properties

    Categories can have most of the same properties that POIs and Trails can
    have, providing defaults for any POI and Trails contained in the category.

    **Columns**

    ======== ======= =========================
    Name     Type    Description
    ======== ======= =========================
    id       INTEGER Unique identifier.
    category TEXT    The category ``typeid``.
    property TEXT    The name of the property.
    value            The property value.
    ======== ======= =========================

    .. versionhistory::
        :0.1.0: Added
]]--
local category_props_create_sql = [[
CREATE TABLE IF NOT EXISTS category_props (
    id INTEGER PRIMARY KEY,
    category TEXT NOT NULL,
    property TEXT NOT NULL,
    value,
    FOREIGN KEY (category) REFERENCES category (typeid) ON DELETE CASCADE,
    UNIQUE (category, property)
)
]]

--[[ RST
.. overlay:dbtable:: poi

    Points of Interest. These are locations shown as markers.

    The location and other attributes are stored as properties in
    the :overlay:dbtable:`markers.poi_props` table.

    **Columns**

    ========== ======= ========================
    Name       Type    Description
    ========== ======= ========================
    id         INTEGER Unique identifier.
    type       TEXT    The category ``typeid``.
    markerpack INTEGER The markerpack id.
    ========== ======= ========================

    .. versionhistory::
        :0.1.0: Added
]]--
local poi_create_sql = [[
CREATE TABLE IF NOT EXISTS poi (
    id INTEGER PRIMARY KEY,
    type TEXT NOT NULL,
    markerpack INTEGER NOT NULL,
    FOREIGN KEY (type) REFERENCES category (typeid) ON DELETE CASCADE,
    FOREIGN KEY (markerpack) REFERENCES markerpack (id) ON DELETE CASCADE
)
]]

--[[ RST
.. overlay:dbtable:: poi_props

    POI Properties

    **Columns**

    ======== ======= ==================
    Name     Type    Description
    ======== ======= ==================
    id       INTEGER Unique identifier.
    poi      INTEGER POI id.
    property TEXT    Property name.
    value            Property value.
    ======== ======= ==================

    .. versionhistory::
        :0.1.0: Added
]]--
local poi_props_create_sql = [[
CREATE TABLE IF NOT EXISTS poi_props (
    id INTEGER PRIMARY KEY,
    poi INTEGER NOT NULL,
    property TEXT NOT NULL,
    value,
    FOREIGN KEY (poi) REFERENCES poi (id) ON DELETE CASCADE,
    UNIQUE (poi, property)
)
]]

--[[ RST
.. overlay:dbtable:: trail

    Trails. These are routes shown as paths.

    **Columns**

    ========== ======= ====================
    Name       Type    Description
    ========== ======= ====================
    id         INTEGER Unique identifier.
    type       TEXT    Category ``typeid``.
    markerpack INTEGER Markerpack id.
    ========== ======= ====================

    .. versionhistory::
        :0.1.0: Added
]]--
local trail_create_sql = [[
CREATE TABLE IF NOT EXISTS trail
(
    id INTEGER PRIMARY KEY,
    type TEXT NOT NULL REFERENCES category (typeid) ON DELETE CASCADE,
    markerpack INTEGER NOT NULL REFERENCES markerpack (id) ON DELETE CASCADE
)
]]

--[[ RST
.. overlay:dbtable:: trail_props

    Trail Properties

    **Columns**

    ======== ======= ==================
    Name     Type    Description
    ======== ======= ==================
    id       INTEGER Unique identifier.
    trail    INTEGER Trail id.
    property TEXT    Property name.
    value            Property value.
    ======== ======= ==================

    .. versionhistory::
        :0.1.0: Added
]]--
local trail_props_create_sql = [[
CREATE TABLE IF NOT EXISTS trail_props (
    id INTEGER PRIMARY KEY,
    trail INTEGER NOT NULL REFERENCES trail (id) ON DELETE CASCADE,
    property TEXT NOT NULL,
    value,
    UNIQUE (trail, property)
)
]]

--[[ RST
.. overlay:dbtable:: trail_coord

    Trail Coordinates

    **Columns**

    ===== ======= ==================
    Name  Type    Description
    ===== ======= ==================
    id    INTEGER Unique identifier.
    seq   INTEGER Sequence number.
    trail INTEGER Trail id.
    x     REAL    x coordinate.
    y     REAL    y coordinate.
    z     REAL    z coordinate.
    ===== ======= ==================

    .. versionhistory::
        :0.1.0: Added
]]--
local trail_coords_create_sql = [[
CREATE TABLE IF NOT EXISTS trail_coord (
    id INTEGER PRIMARY KEY,
    seq INTEGER NOT NULL,
    trail INTEGER NOT NULL REFERENCES trail (id) ON DELETE CASCADE,
    x REAL NOT NULL,
    y REAL NOT NULL,
    z REAL NOT NULL,
    UNIQUE (trail, seq)
)
]]

--[[ RST
.. overlay:dbtable:: data_file

    Data Files

    These will primarly be texture/images. Trail data files are loaded directly
    to :overlay:dbtable:`markers.trail_coord`.

    **Columns**

    ==== ==== ==========================================================================================
    Name Type Description
    ==== ==== ==========================================================================================
    path TEXT The path to the data file. This will be a relative path and serves as a unique identifier.
    data BLOB The binary data of the file.
    ==== ==== ==========================================================================================

    .. versionhistory::
        :0.1.0: Added
]]--
local data_file_create_sql = [[
CREATE TABLE IF NOT EXISTS data_file (
    path TEXT PRIMARY KEY,
    data BLOT NOT NULL
)
]]

data.db:execute(markerpack_create_sql)
data.db:execute(category_create_sql)
data.db:execute(category_props_create_sql)
data.db:execute(poi_create_sql)
data.db:execute(poi_props_create_sql)
data.db:execute(trail_create_sql)
data.db:execute(trail_props_create_sql)
data.db:execute(trail_coords_create_sql)
data.db:execute(data_file_create_sql)

return data
