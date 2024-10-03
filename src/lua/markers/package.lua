--[[ RST
markers.package
===============

.. lua:module:: markers.package

]]--

local overlay = require 'eg-overlay'
local sqlite = require 'sqlite'
local logger = require 'logger'

local log = logger.logger:new('markers.package')

local M = {}

--[[ RST
Classes
-------

.. lua:class:: markerpack

    A markerpack is a collection of categories, POIs, trails and associated
    data.
]]--
M.markerpack = {}
M.markerpack.__index = M.markerpack

--[[ RST
    .. lua:method:: new(path)

        Open or create a new markerpack. If ``path`` does not exist, a new
        markerpack at that location will be created.

        If ``path`` does exist, it will be verified as a valid markerpack.

        If ``path`` can not be created or exists and is not a valid markerpack
        an error is raised.

        :param string path:
        :rtype: markerpack

        .. admonition:: Implementation Detail

            All data for the markerpack is stored in a SQLite database. ``path``
            is the path to this database and should end in ``.db``.

        .. versionhistory::
            :0.1.0: Added
]]--
function M.markerpack:new(path)
    local mp = {}
    
    local dbok, db = pcall(sqlite.open, path)

    if not dbok then
        error(string.format("Couldn't open markerpack: %s", db), 2)
    end

    mp.db = db
    mp.path = path

    setmetatable(mp, self)

    if not mp:verifydata() then
        error(string.format("Couldn't verify markerpack data for %s", path), 2)
    end

    -- turn on foreign key checks
    mp.db:execute('PRAGMA foreign_keys = ON')

    mp.db:execute("PRAGMA journal_mode = WAL")
    mp.db:execute("PRAGMA synchronous = NORMAL")

    return mp
end

--[[ RST
    .. lua:method:: category(typeid[, create])
        
        Retrieve a category from this markerpack. If ``create`` is true, it will
        be created if it does not exist. 

        :param string typeid:
        :param boolean create: (Optional)
        :rtype: category

        .. versionhistory::
            :0.1.0: Added
]]--
function M.markerpack:category(typeid, create)
    local cat = {db = self.db, typeid = typeid, markerpack = self}
    
    if create then
        local parent, child = string.match(typeid, '([%w._%-]*)%.([%w%-_]+)')

        local seq = nil
        if parent then
            local s = self.db:prepare([[
                SELECT MAX(seq) + 1 as next FROM categories
                WHERE parent = ?
            ]])
            s:bind(1, parent)
            local r = s:step()
            s:finalize()
            if r then
                seq = r.next
            end
        end

        -- do nothing on unique constraint conflict
        -- that means this category already exists
        local sql = [[
            INSERT INTO categories (typeid, parent, seq) VALUES (?, ?, ?)
            ON CONFLICT (typeid) DO NOTHING
        ]]
        local s = self.db:prepare(sql)
        s:bind(1, typeid)
        s:bind(2, parent)
        s:bind(3, seq)
        s:step()
        s:finalize()
    end

    setmetatable(cat, M.category)
    return cat
end

--[[ RST
    .. lua:method:: categorycount()

        Return the total number of categories in this markerpack.

        :rtype: integer

        .. versionhistory::
            :0.1.0: Added
]]--
function M.markerpack:categorycount()
    local r = self.db:execute([[ SELECT COUNT(*) AS count FROM categories ]])
    if r then return r.count end
end

function M.markerpack:verifydata()
    self.db:execute([[
        CREATE TABLE IF NOT EXISTS markerpack (
            version INTEGER NOT NULL
        )
    ]])

    local s = self.db:prepare([[ SELECT version FROM markerpack ]])
    local r = s:step()
    s:finalize()

    if r==nil then
        self.db:execute([[ INSERT INTO markerpack (version) VALUES (1) ]])
    else
        if r.version~=1 then
            log:error("Unknown markerpack version %d (%s)", self.path)
            return false
        end
    end

    self.db:execute([[
        CREATE TABLE IF NOT EXISTS categories (
            typeid TEXT PRIMARY KEY NOT NULL,
            parent TEXT REFERENCES categories (typeid) ON DELETE CASCADE,
            active BOOL NOT NULL DEFAULT FALSE,
            seq INTEGER
        )
    ]])

    self.db:execute([[
        CREATE TABLE IF NOT EXISTS categoryprops (
            id INTEGER PRIMARY KEY,
            category TEXT NOT NULL REFERENCES categories (typeid) ON DELETE CASCADE,
            property TEXT NOT NULL,
            value
        )
    ]])
    self.db:execute([[ 
        CREATE UNIQUE INDEX IF NOT EXISTS idx_categoryprops_prop
        ON categoryprops (category, property)
    ]])

    self.db:execute([[
        CREATE TABLE IF NOT EXISTS pois (
            id INTEGER PRIMARY KEY,
            type TEXT NOT NULL REFERENCES categories (typeid) ON DELETE CASCADE
        )
    ]])
    self.db:execute([[ CREATE INDEX IF NOT EXISTS idx_pois_type ON pois (type) ]])

    self.db:execute([[
        CREATE TABLE IF NOT EXISTS poiprops (
            id INTEGER PRIMARY KEY,
            poi INTEGER NOT NULL REFERENCES poi (id) ON DELETE CASCADE,
            property TEXT NOT NULL,
            value
        )
    ]])
    self.db:execute([[ CREATE UNIQUE INDEX IF NOT EXISTS idx_poiprops_poi ON poiprops (poi, property) ]])

    self.db:execute([[
        CREATE TABLE IF NOT EXISTS trails (
            id INTEGER PRIMARY KEY,
            type TEXT NOT NULL REFERENCES categories (typeid) ON DELETE CASCADE
        )
    ]])

    self.db:execute([[
        CREATE TABLE IF NOT EXISTS trailprops (
            id INTEGER PRIMARY KEY,
            trail INTEGER NOT NULL REFERENCES trails (id) ON DELETE CASCADE,
            property TEXT NOT NULL,
            value
        )
    ]])
    self.db:execute([[ CREATE UNIQUE INDEX IF NOT EXISTS idx_trailprops_trail on trailprops (trail, property) ]])

    self.db:execute([[
        CREATE TABLE IF NOT EXISTS trailcoords (
            id INTEGER PRIMARY KEY,
            seq INTEGER NOT NULL,
            trail INTEGER NOT NULL REFERENCES trails (id) ON DELETE CASCADE,
            x REAL NOT NULL,
            y REAL NOT NULL,
            z REAL NOT NULL
        )
    ]])
    self.db:execute([[ CREATE UNIQUE INDEX IF NOT EXISTS idx_trailcoords_seq ON trailcoords (trail, seq) ]])

    self.db:execute([[
        CREATE TABLE IF NOT EXISTS datafiles (
            path TEXT PRIMARY KEY,
            data BLOB NOT NULL
        )
    ]])

    return true
end

function M.markerpack:__tostring()
    return string.format('markers.package.markerpack: %s', self.path)
end

--[[ RST
.. lua:class:: category

    A category within a markerpack. A category is a named grouping of POIs and
    trails and is the primary method users use to control the visibility of
    those elements.

    Categories are also used to set properties of POIs and trails contained
    within them.

    Category properties can be accessed directly as fields on the category
    object. Setting a field to ``nil`` will remove it completely.

    .. code-block:: lua
        :caption: Example

        local mp = require 'markers.package'
        local pack = mp.markerpack:new('data/markers/test.db')

        -- create a new category
        local cat = pack:category('newcategory', true)

        -- set properties
        cat.displayName = 'A new category'

        -- or access them like this
        cat['displayname'] = 'Another category'

        -- access properties
        print(cat.displayName)

        -- properties are not case sensitive
        cat.DISPLAYNAME = 'Hello EG-Overlay'
        local foo = cat.displayname -- 'Hello EG-Overlay'

        -- active is not a property
        cat:active(true)
        local isactive = cat:active()
]]--
M.category = {}

--[[ RST
    .. lua:method:: active([value])
        
        Get or set if this category is active (shown).

        :param boolean value: (Optional)
        :rtype: boolean

        .. versionhistory::
            :0.1.0: Added
]]--
function M.category:active(value)
    if value~=nil then
        local s = self.db:prepare([[ UPDATE categories SET active = ? WHERE typeid = ? ]])
        s:bind(1, value)
        s:bind(2, self.typeid)
        s:step()
        s:finalize()
    else
        local s = self.db:prepare([[ SELECT active FROM categories WHERE typeid = ? ]])
        s:bind(1, self.typeid)
        local r = s:step()
        s:finalize()

        return r.active
    end
end

--[[ RST
    .. lua:method:: delete()

        Delete this category.

        .. danger::

            This will remove this category, all of its stored properties and
            associated POIs and trails.

            Once this method has been invoked this object and any related
            objects become invalid. Attempting to use them will raise an error.

        .. versionhistory::
            :0.1.0: Added
]]--
function M.category:delete()
    local s = self.db:prepare([[ DELETE FROM categories WHERE typeid = ? ]])
    s:bind(1, self.typeid)
    s:step()
    s:finalize()

    self.db = nil
    self.typeid = nil
end

--[[ RST
    .. lua:method:: newpoi()

        Create a new :lua:class:`poi` in this category.

        :rtype: poi

        .. versionhistory::
            :0.1.0: Added
]]--
function M.category:newpoi()
    return M.poi:new(self)
end

--[[ RST
    .. lua:method:: parent()

        Return the parent category of this category or ``nil``.

        :rtype: category

        .. versionhistory::
            :0.1.0: Added
]]--
function M.category:parent()
    local s = self.db:prepare([[ SELECT parent FROM categories WHERE typeid = ? ]])
    s:bind(1, self.typeid)
    local r = s:step()
    if r and r.parent then
        return self.markerpack:category(r.parent)
    end

    return nil
end

function M.category:__index(key)
    if M.category[key] and type(M.category[key])=='function' then
        return M.category[key]
    end

    local s = self.db:prepare([[ SELECT value FROM categoryprops WHERE category = ? AND property = ? ]])
    s:bind(1, self.typeid)
    s:bind(2, string.lower(key))

    local r = s:step()
    s:finalize()

    if r then return r.value end

    local parent = self:parent()
    if parent then
        return parent[key]
    end

    return nil
end

function M.category:__newindex(key, value)
    if M.category[key] and type(M.category[key])=='function' then
        error(string.format('%s is a function', key), 2)
    end

    if value==nil then
        local s = self.db:prepare([[ DELETE FROM categoryprops WHERE category = ? AND property = ? ]])
        s:bind(1, self.typeid)
        s:bind(2, string.lower(key))
        s:step()
        s:finalize()
    else
        local s = self.db:prepare([[
            INSERT INTO categoryprops (category, property, value)
            VALUES (?, ?, ?)
            ON CONFLICT(category, property) DO UPDATE SET value=excluded.value
        ]])
        s:bind(1, self.typeid)
        s:bind(2, string.lower(key))
        s:bind(3, value)
        s:step()
        s:finalize()
    end
end

function M.category:__tostring()
    return string.format('markers.package.category: %s', self.typeid)
end

--[[ RST
.. lua:class:: poi
    
    A Point of Interest. POIs are the points at which markers are displayed.
    POIs belong to a :lua:class:`category` and have properties. Any properties
    that a POI does not define will be derived from properties of the category
    that contains it.
]]--
M.poi = {}

function M.poi:new(category, id)
    local p = { db = category.db, category = category }

    if id~=nil then
        local s = p.db:prepare([[ SELECT id, type FROM pois WHERE id = ? ]])
        s:bind(1, id)
        local r = s:step()
        s:finalize()

        if r.id~=id or r.type~=category.typeid then
            error(string.format("Unknown POI id: %d", id))
        end
        p.id = id
    else
        local s = p.db:prepare([[ INSERT INTO pois (type) VALUES (?) RETURNING id ]])
        s:bind(1, category.typeid)
        r = s:step()
        s:finalize()

        p.id = r.id

        if p.id==nil then
            error("Error inserting POI")
        end
    end

    setmetatable(p, self)

    return p
end

function M.poi:delete()
    local s = self.db:prepare([[ DELETE FROM pois WHERE id = ? ]])
    s:bind(1, self.id)
    s:step()
    s:finalize()

    self.db = nil
    self.id = nil
    self.category = nil
end

function M.poi:__index(key)
    if M.poi[key] and type(M.poi[key])=='function' then return M.poi[key] end

    local s = self.db:prepare([[ SELECT value FROM poiprops WHERE poi = ? AND property = ? ]])
    s:bind(1, self.id)
    s:bind(2, string.lower(key))

    local r = s:step()
    s:finalize()

    if r then return r.value end
    return self.category[key]
end

function M.poi:__newindex(key, value)
    if value==nil then
        local s = self.db:prepare([[ DELETE FROM poiprops WHERE poi = ? AND property = ? ]])
        s:bind(1, self.id)
        s:bind(2, string.lower(key))
        s:step()
        s:finalize()
    else
        local s = self.db:prepare([[
            INSERT INTO poiprops (poi, property, value)
            VALUES (?, ?, ?)
            ON CONFLICT(poi, property) DO UPDATE SET value=excluded.value
        ]])
        s:bind(1, self.id)
        s:bind(2, string.lower(key))
        s:bind(3, value)
        s:step()
        s:finalize()
    end
end

function M.poi:__tostring()
    return string.format('markers.package.poi: [%s] %d', self.category.typeid, self.id)
end

return M
