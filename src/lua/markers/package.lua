--[[ RST
markers.package
===============

.. lua:module:: markers.package

]]--

local sqlite = require 'sqlite'
local logger = require 'logger'
local settings = require 'markers.settings'

local log = logger.logger:new('markers.package')

settings:setdefault('markerpackCacheSize', -2000)

local M = {}

--[[ RST
Classes
-------

.. lua:class:: markerpack

    A markerpack is a collection of categories, markers, trails and associated 
    data.

    .. lua:attribute:: path: string

        The path of this marker pack, specified when creating it.

        .. versionhistory::
            :0.1.0: Added

    .. lua:attribute:: db: sqlitedb

        The underlying SQLite database.

        .. versionhistory::
            :0.1.0: Added
]]--
M.markerpack = {}
M.markerpack.__index = M.markerpack

--[[ RST
    .. lua:method:: open(path)

        Similar to :lua:meth:`new` but does not create a new markerpack if
        ``path`` does not exist. In that case ``nil`` is returned.

        :param string path:
        :rtype: markerpack

        .. versionhistory::
            :0.1.0: Added
]]--
function M.markerpack:open(path)
    local f = io.open(path, 'rb')
    if f then
        f:close()
        return M.markerpack:new(path)
    else
        log:error("Can't open markerpack, path does not exist: %s", path)
        return nil
    end
end

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
    else
        log:debug("Loaded %s", path)
    end

    local cache_size = settings:get('markerpackCacheSize')
    mp.db:execute(string.format('PRAGMA cache_size = %d', cache_size))

    -- turn on foreign key checks
    mp.db:execute('PRAGMA foreign_keys = ON')
    mp.db:execute('PRAGMA optimize=0x10002')
    mp.db:execute("PRAGMA journal_mode = WAL")
    mp.db:execute("PRAGMA synchronous = NORMAL")

    -- prepare all statements once and use them over and over again
    mp.statements = {
        markerpack = {},
        category = {},
        marker = {},
        trail = {},
        datafile = {},
    }

    mp.statements.markerpack.catsinmap = db:prepare([[
        SELECT DISTINCT type AS typeid FROM markers WHERE mapid = :mapid
        UNION
        SELECT DISTINCT type AS typeid FROM trails WHERE mapid = :mapid
    ]])
    mp.statements.markerpack.activecats = db:prepare([[
        WITH RECURSIVE
            actives (typeid) AS (
                SELECT typeid FROM categories
                WHERE parent IS NULL AND active = TRUE
                UNION ALL
                SELECT child.typeid FROM categories AS child, actives AS parent
                WHERE child.parent = parent.typeid
                  AND child.active = TRUE
            )
        SELECT * FROM actives
    ]])

    -- categories --
    mp.statements.category.insert = db:prepare([[
        INSERT INTO categories (typeid, parent, seq) VALUES (:typeid, :parent, :seq)
        ON CONFLICT (typeid) DO NOTHING
    ]])
    mp.statements.category.selectbytypeid = db:prepare([[ SELECT typeid FROM categories WHERE typeid = :typeid ]])
    mp.statements.category.nextseq = db:prepare([[
        SELECT MAX(seq) + 1 as next FROM categories
        WHERE parent = :parent
    ]])
    --mp.statements.category.setactive = db:prepare([[ UPDATE categories SET active = :active WHERE typeid = :typeid ]])
    --mp.statements.category.getactive = db:prepare([[ SELECT active FROM categories WHERE typeid = :typeid ]])
    mp.statements.category.hasmarkersinmap = db:prepare([[
        WITH trailsmarkers (count) AS (
            SELECT COUNT(trails.id) AS count
            FROM trails
            WHERE (trails.type LIKE :typeidlike OR trails.type = :typeid)
              AND trails.mapid = :mapid
            UNION ALL
            SELECT COUNT(markers.id) AS count
            FROM markers
            WHERE (markers.type LIKE :typeidlike OR markers.type = :typeid)
              AND markers.mapid = :mapid
        ) 
        SELECT SUM(count) AS count
        FROM trailsmarkers
    ]])
    mp.statements.category.childcount = db:prepare([[ SELECT COUNT(*) AS count FROM categories WHERE parent = :parent ]])
    mp.statements.category.children = db:prepare([[
        SELECT typeid
        FROM categories
        WHERE parent = :parent
        ORDER BY seq
    ]])
    mp.statements.category.delete = db:prepare([[ DELETE FROM categories WHERE typeid = :typeid ]])
    mp.statements.category.newmarker = db:prepare([[ INSERT INTO markers (type) VALUES (:typeid) RETURNING id ]])
    mp.statements.category.newtrail = db:prepare([[ INSERT INTO trails (type) VALUES (:typeid) RETURNING id ]])
    mp.statements.category.getparent = db:prepare([[ SELECT parent FROM categories WHERE typeid = :typeid ]])
    mp.statements.category.trailsinmap = db:prepare([[
        SELECT trails.id FROM trails
        WHERE trails.type = :category
          AND trails.mapid = :mapid
    ]])
    mp.statements.category.markersinmap = db:prepare([[
        SELECT markers.id FROM markers
        WHERE markers.type = :category
          AND markers.mapid = :mapid
    ]]) 
    mp.statements.category.getprop = db:prepare([[
        WITH RECURSIVE allcats AS (
            SELECT typeid, parent FROM categories WHERE typeid = :typeid
            UNION ALL
            SELECT p.typeid, p.parent FROM categories p, allcats c WHERE c.parent = p.typeid
        ), allprops AS (
            SELECT value
            FROM allcats
            LEFT JOIN categoryprops
                ON categoryprops.category = allcats.typeid
                AND categoryprops.property = :property
        )
        SELECT value
        FROM allprops
        WHERE value IS NOT NULL
        LIMIT 1
    ]])
    mp.statements.category.setprop = db:prepare([[
        INSERT INTO categoryprops (category, property, value)
        VALUES (:typeid, :property, :value)
        ON CONFLICT(category, property) DO UPDATE SET value=excluded.value
    ]])
    mp.statements.category.delprop = db:prepare([[
        DELETE FROM categoryprops WHERE category = :typeid AND property = :property
    ]])

    -- markers --
    mp.statements.marker.getprop = db:prepare([[
        SELECT value FROM markerprops WHERE marker = :id AND property = :property
    ]])
    mp.statements.marker.setprop = db:prepare([[
        INSERT INTO markerprops (marker, property, value)
        VALUES (:id, :property, :value)
        ON CONFLICT(marker, property) DO UPDATE SET value=excluded.value
    ]])
    mp.statements.marker.delprop = db:prepare([[ DELETE FROM markerprops WHERE marker = :id AND property = :property ]])
    mp.statements.marker.setmapid = db:prepare([[ UPDATE markers SET mapid = :mapid WHERE id = :id ]])
    mp.statements.marker.getmapid = db:prepare([[ SELECT mapid FROM markers WHERE id = :id ]])
    mp.statements.marker.delete = db:prepare([[ DELETE FROM markers WHERE id = :id ]])

    -- trails --
    mp.statements.trail.setmapid = db:prepare([[ UPDATE trails SET mapid = :mapid WHERE id = :id ]])
    mp.statements.trail.getmapid = db:prepare([[ SELECT mapid FROM trails WHERE id = :id ]])
    mp.statements.trail.maxcoordseq = db:prepare([[ SELECT MAX(seq) + 1 AS newseq FROM trailcoords WHERE trail = :id ]])
    mp.statements.trail.addcoord = db:prepare([[
        INSERT INTO trailcoords (trail, seq, x, y, z)
        VALUES (:id, :seq, :x, :y, :z)
    ]])
    mp.statements.trail.coords = db:prepare([[
        SELECT x, y, z
        FROM trailcoords
        WHERE trail = :id
        ORDER BY seq ASC
    ]])
    mp.statements.trail.delete = db:prepare([[ DELETE FROM trails WHERE id = :id ]])
    mp.statements.trail.getprop = db:prepare([[ SELECT value FROM trailprops WHERE trail = :id and property = :property ]])
    mp.statements.trail.delprop = db:prepare([[ DELETE FROM trailprops WHERE trail = :id AND property = :property ]])
    mp.statements.trail.setprop = db:prepare([[
            INSERT INTO trailprops (trail, property, value)
            VALUES (:id, :property, :value)
            ON CONFLICT(trail, property) DO UPDATE SET value=excluded.value
        ]])

    -- datafile --
    mp.statements.datafile.setdata = db:prepare([[
        INSERT INTO datafiles (path, data)
        VALUES (:path, :data)
        ON CONFLICT(path) DO UPDATE SET data=excluded.data
    ]])
    mp.statements.datafile.getdata = db:prepare([[ SELECT data FROM datafiles WHERE path = :path ]])
    mp.statements.datafile.delete = db:prepare([[ DELETE FROM datafiles WHERE path = :path ]])

    return mp
end

--[[ RST
    .. lua:method:: category(typeid[, create])
        
        Retrieve a category from this markerpack. If ``create`` is true, it will
        be created if it does not exist. If ``create`` is false and the category
        does not already exist, ``nil`` is returned instead.

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
            local s = self.statements.category.nextseq
            s:reset()
            s:bind(':parent', parent)
            local r = s:step()
            s:reset()

            if r then
                seq = r.next or 1
            else
                seq = 1
            end
        end

        -- do nothing on unique constraint conflict
        -- that means this category already exists
        local s = self.statements.category.insert
        s:reset()
        s:bind(':typeid', typeid)
        s:bind(':parent', parent)
        s:bind(':seq', seq)
        s:step()
        s:reset()
    else
        local s = self.statements.category.selectbytypeid
        s:reset()
        s:bind(':typeid', typeid)

        local r = s:step()
        s:reset()
        if r==nil then return nil end
    end

    setmetatable(cat, M.category)
    return cat
end

--[[ RST
    .. lua:method:: toplevelcategories()

        Return a sequence of all of the top level categories in this markerpack.

        :rtype: sequence

        .. versionhistory::
            :0.1.0: Added
]]--
function M.markerpack:toplevelcategories()
    local s = self.db:prepare([[ SELECT typeid FROM categories WHERE parent IS NULL ]])
    local cats = {}

    local r = s:step()
    while r do
        table.insert(cats, self:category(r.typeid))
        r = s:step()
    end
    s:reset()

    return cats
end

--[[ RST
    .. lua:method:: toplevelcategoriesiter()

        Returns an iterator function that will return a :lua:class:`category`
        on each invocation until all top level categories in this markerpack
        have been returned, at which point ``nil`` is returned.

        :rtype: category

        .. versionhistory::
            :0.1.0: Added
]]--
function M.markerpack:toplevelcategoriesiter()
    local s = self.db:prepare([[ SELECT typeid FROM categories WHERE parent IS NULL ]])

    local iter = function()
        local r = s:step()
        if r then
            return self:category(r.typeid)
        else
            s:finalize()
            return nil
        end
    end

    return iter
end

function M.markerpack:categoriesinmapiter(mapid)
    local s = self.statements.markerpack.catsinmap
    s:reset()
    s:bind(':mapid', mapid)

    local iter = function()
        local r = s:step()
        if r then
            return self:category(r.typeid)
        else
            s:reset()
            return nil
        end
    end

    return iter
end

--[[ RST
    .. lua:method:: activecategoriesiter()

        Returns an iterator function that will return a :lua:class:`category`
        on each invocation until all active categories in this markerpack have
        been returned, at which point ``nil`` is returned.

        :rtype: function
        
        .. versionhistory::
            :0.1.0: Added
]]--
--[[
function M.markerpack:activecategoriesiter()
    -- this is a recursive query that starts at the top level categories,
    -- returns those that are active and then goes one by one returning just
    -- the active children of the active parents, and so on. 
    -- This way, only the categories who are both active and have active
    -- ancestors are retrieved in an efficient way.
    local s =  self.statements.markerpack.activecats
    s:reset()
    local iter = function()
        local r = s:step()
        if r then
            return self:category(r.typeid)
        else
            s:reset()
            return nil
        end
    end

    return iter
end
]]

--[[ RST
    .. lua:method:: datafile(path)

        Get a data file in this markerpack.

        :param string path:
        :rtype: datafile

        .. versionhistory::
            :0.1.0: Added
]]--
function M.markerpack:datafile(path)
    local df = {}
    df.markerpack = self
    df.db = self.db
    df.path = path

    setmetatable(df, M.datafile)

    return df
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
        ) WITHOUT ROWID
    ]])
    self.db:execute([[ CREATE INDEX IF NOT EXISTS idx_categories_active ON categories (active) ]])

    self.db:execute([[
        CREATE TABLE IF NOT EXISTS categoryprops (
            category TEXT NOT NULL REFERENCES categories (typeid) ON DELETE CASCADE,
            property TEXT NOT NULL,
            value,
            PRIMARY KEY (category, property)
        ) WITHOUT ROWID
    ]])

    self.db:execute([[
        CREATE TABLE IF NOT EXISTS markers (
            id INTEGER PRIMARY KEY,
            type TEXT NOT NULL REFERENCES categories (typeid) ON DELETE CASCADE,
            mapid INTEGER
        )
    ]])
    self.db:execute([[ CREATE INDEX IF NOT EXISTS idx_markers_type ON markers (type) ]])
    self.db:execute([[ CREATE INDEX IF NOT EXISTS idx_markers_mapid ON markers (mapid) ]])

    self.db:execute([[
        CREATE TABLE IF NOT EXISTS markerprops (
            marker INTEGER NOT NULL REFERENCES markers (id) ON DELETE CASCADE,
            property TEXT NOT NULL,
            value,
            PRIMARY KEY (marker, property)
        ) WITHOUT ROWID
    ]])

    self.db:execute([[
        CREATE TABLE IF NOT EXISTS trails (
            id INTEGER PRIMARY KEY,
            type TEXT NOT NULL REFERENCES categories (typeid) ON DELETE CASCADE,
            mapid INTEGER
        )
    ]])
    self.db:execute([[ CREATE INDEX IF NOT EXISTS idx_trails_type ON trails (type) ]])
    self.db:execute([[ CREATE INDEX IF NOT EXISTS idx_trails_mapid ON trails (mapid) ]])

    self.db:execute([[
        CREATE TABLE IF NOT EXISTS trailprops (
            trail INTEGER NOT NULL REFERENCES trails (id) ON DELETE CASCADE,
            property TEXT NOT NULL,
            value,
            PRIMARY KEY (trail, property)
        ) WITHOUT ROWID
    ]])

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
        ) WITHOUT ROWID
    ]])

    return true
end

function M.markerpack:__tostring()
    return string.format('markerpack: %s', self.path)
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

    .. lua:attribute:: typeid: string
        
        This category's typeid, its unique ID.

        .. versionhistory::
            :0.1.0: Added

    .. lua:attribute:: markerpack: markerpack

        The :lua:class:`markerpack` this category belongs to.

        .. versionhistory::
            :0.1.0: Added
]]--
M.category = {}

--[[ RST
    .. lua:method:: ancestorsactive()

        Returns true if this category and all of its ancestors are active.

        :rtype: boolean

        .. versionhistory::
            :0.1.0: Added
]]--
--[[
function M.category:ancestorsactive()
    local c = self

    while c~=nil do
        if not c:active() then return false end
        c = c:parent()
    end

    return true
end
]]

--[[ RST
    .. lua:method:: active([value])
        
        Get or set if this category is active (shown).

        :param boolean value: (Optional)
        :rtype: boolean

        .. versionhistory::
            :0.1.0: Added
]]--
--[[
function M.category:active(value)
    if value~=nil then
        local s = self.markerpack.statements.category.setactive
        s:reset()
        s:bind(':active', value)
        s:bind(':typeid', self.typeid)
        s:step()
        s:reset()
    else
        local s = self.markerpack.statements.category.getactive
        s:reset()
        s:bind(':typeid', self.typeid)
        local r = s:step()
        s:reset()

        return r.active == 1
    end
end
]]

--[[ RST
    .. lua:method:: hasmarkersinmap(mapid[, includechildren])

        Return true if this category, or its descendants if ``includechildren``
        is ``true``, have markers or trails in ``mapid``.

        :param boolean includechildren:
        :rtype: boolean

        .. versionhistory::
            :0.1.0: Added
]]--
function M.category:hasmarkersinmap(mapid, includechildren)
    local s = self.markerpack.statements.category.hasmarkersinmap
    s:reset()
    if includechildren then
        s:bind(':typeidlike', self.typeid .. '.%')
    else
        s:bind(':typeidlike', self.typeid)
    end
    s:bind(':typeid', self.typeid)
    s:bind(':mapid', mapid)

    local r = s:step()
    s:reset()

    if r and r.count > 0 then return true end

    -- no trails or markers
    return false
end

--[[ RST
    .. lua:method:: childcount()

        Return the number of children of this category.

        :rtype: integer

        .. versionhistory::
            :0.1.0: Added
]]--
function M.category:childcount()
    local s = self.markerpack.statements.category.childcount
    s:reset()
    s:bind(':parent', self.typeid)
    
    local r = s:step()
    s:reset()
    if not r then
        error("Couldn't get category children.", 2)
    end

    return r.count
end

--[[ RST
    .. lua:method:: children()

        Return the immediate children of this category, as a sequence of
        :lua:class:`category`.

        :rtype: sequence

        .. versionhistory::
            :0.1.0: Added
]]--
function M.category:children()
    local children = {}

    for child in self:childreniter() do
        table.insert(children, child)
    end

    return children
end

--[[ RST
    .. lua:method:: childreniter()

        Return a function that will return the immediate children of this
        category on each invocation.

        :rtype: function

        .. versionhistory::
            :0.1.0: Added
]]--
function M.category:childreniter()
    local s = self.markerpack.statements.category.children
    s:reset()
    s:bind(':parent', self.typeid)
    
    local iter = function()
        local r = s:step()
        if r then
            return self.markerpack:category(r.typeid)
        else
            s:reset()
            return nil
        end
    end

    return iter
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
    local s = self.markerpack.statements.category.delete
    s:reset()
    s:bind(':typeid', self.typeid)
    s:step()
    s:reset()

    self.db = nil
    self.typeid = nil
end

--[[ RST
    .. lua:method:: newmarker()

        Create a new :lua:class:`marker` in this category.

        :rtype: marker

        .. versionhistory::
            :0.1.0: Added
]]--
function M.category:newmarker()
    local p = { db = self.db, category = self }

    local s = self.markerpack.statements.category.newmarker
    s:reset()
    s:bind(':typeid', self.typeid)
    local r = s:step()

    p.id = r.id

    -- put the statement into a finished mode
    s:reset()

    setmetatable(p, M.marker)

    return p
end

--[[ RST
    .. lua:method:: newtrail()

        Create a new :lua:class:`trail` in this category.

        :rtype: trail

        .. versionhistory::
            :0.1.0: Added
]]--
function M.category:newtrail()
    local t = { db = self.db, category = self }

    local s = self.markerpack.statements.category.newtrail
    s:reset()
    s:bind(1, self.typeid)
    local r = s:step()

    t.id = r.id

    -- put the statement into a finished mode
    s:reset()


    setmetatable(t, M.trail)

    return t
end

--[[ RST
    .. lua:method:: parent()

        Return the parent category of this category or ``nil``.

        :rtype: category

        .. versionhistory::
            :0.1.0: Added
]]--
function M.category:parent()
    local s = self.markerpack.statements.category.getparent
    s:reset()
    s:bind(':typeid', self.typeid)
    local r = s:step()
    s:reset()
    if r and r.parent then
        return self.markerpack:category(r.parent)
    end

    return nil
end

--[[ RST
    .. lua:method:: markersinmapiter(mapid)

        Return an iterator function that returns a :lua:class:`marker` each time it
        is invoked until all markerss in this category in the given ``mapid`` are
        returned.

        :rtype: marker

        .. versionhistory::
            :0.1.0: Added
]]--
function M.category:markersinmapiter(mapid)
    local s = self.markerpack.statements.category.markersinmap
    s:reset()
    s:bind(':category', self.typeid)
    s:bind(':mapid', mapid)

    local iter = function()
        local r = s:step()
        if r then
            local marker = { id = r.id, db = self.db, category = self }
            setmetatable(marker, M.marker)
            return marker
        else
            s:reset()
            return nil
        end
    end

    return iter
end

--[[ RST
    .. lua:method:: trailsinmapiter(mapid)

        Return an iterator that returns a :lua:class:`trail` each time it is
        invoked until all trails in the category in the given ``mapid`` are
        returned.

        :type: trail

        .. versionhistory::
            :0.1.0: Added
]]--
function M.category:trailsinmapiter(mapid)
    local s = self.markerpack.statements.category.trailsinmap
    s:reset()
    s:bind(':category', self.typeid)
    s:bind(':mapid', mapid)

    local iter = function()
        local r = s:step()
        if r then
            local trail = { id = r.id, db = self.db, category = self }
            setmetatable(trail, M.trail)
            return trail
        else
            s:reset()
            return nil
        end
    end

    return iter
end

function M.category:__index(key)
    if M.category[key] and type(M.category[key])=='function' then
        return M.category[key]
    end

    local s = self.markerpack.statements.category.getprop
    s:reset()
    s:bind(':typeid', self.typeid)
    s:bind(':property', string.lower(key))

    local r = s:step()
    s:reset()

    if r then return r.value end
end

function M.category:__newindex(key, value)
    if M.category[key] and type(M.category[key])=='function' then
        error(string.format('%s is a function', key), 2)
    end

    if value==nil then
        local s = self.markerpack.statements.category.delprop
        s:reset()
        s:bind(':typeid', self.typeid)
        s:bind(':property', string.lower(key))
        s:step()
        s:reset()
    else
        local s = self.markerpack.statements.category.setprop
        s:reset()
        s:bind(':typeid', self.typeid)
        s:bind(':property', string.lower(key))
        s:bind(':value', value)
        s:step()
        s:reset()
    end
end

function M.category:__tostring()
    return string.format('category: %s', self.typeid)
end

--[[ RST
.. lua:class:: marker
    
    A marker is a point at which an icon is displayed in the GW2 scene to mark
    the location of something. Markers belong to a :lua:class:`category` and
    have properties. Any properties that a marker does not define will be
    derived from properties of the category that contains it.

    .. lua:attribute:: category: category

        The :lua:class:`category` this marker belongs to.

        .. versionhistory::
            :0.1.0: Added
]]--
M.marker = {}

--[[ RST
    .. lua:method:: mapid([value])

        Get or set the MapID of this marker. If ``value`` is present, it is the
        new MapID. Otherwise the current MapID is returned.

        :param integer value: (Optional) The new MapID
        :rtype: integer
        :returns: The current MapID

        .. versionhistory::
            :0.1.0: Added
]]--
function M.marker:mapid(value)
    if value then
        local s = self.category.markerpack.statements.marker.setmapid
        s:reset()
        s:bind(':mapid', value)
        s:bind(':id', self.id)
        s:step()
        s:reset()
    else
        local s = self.category.markerpack.statements.marker.getmapid
        s:reset()
        s:bind(':id', self.id)
        local r = s:step()
        s:reset()
        if r then return r.mapid end
    end
end

--[[ RST
    .. lua:method:: delete()

        Delete this POI and all associated properties.

        .. danger::
            
            This object will no longer be valid after calling this method.

        .. versionhistory::
            :0.1.0: Added
]]--
function M.marker:delete()
    local s = self.category.markerpack.statements.marker.delete
    s:reset()
    s:bind(':id', self.id)
    s:step()
    s:reset()

    self.db = nil
    self.id = nil
    self.category = nil
end

-- undocumented, this only exists to allow access to properties that are also
-- method names, i.e. mapid
function M.marker:getproperty(key)
    local s = self.category.markerpack.statements.marker.getprop
    s:reset()
    s:bind(':id', self.id)
    s:bind(':property', string.lower(key))
    
    local r = s:step()
    s:reset()
    if r then return r.value end

    return self.category[key]
end

function M.marker:__index(key)
    if M.marker[key] and type(M.marker[key])=='function' then return M.marker[key] end
    
    return M.marker.getproperty(self, key)
end

function M.marker:__newindex(key, value)
    if value==nil then
        local s = self.category.markerpack.statements.marker.delprop
        s:reset()
        s:bind(':id', self.id)
        s:bind(':property', string.lower(key))
        s:step()
        s:reset()
    else
        local s = self.category.markerpack.statements.marker.setprop
        s:reset()
        s:bind(':id', self.id)
        s:bind(':property', string.lower(key))
        s:bind(':value', value)
        s:step()
        s:reset()
    end
end

function M.marker:__tostring()
    return string.format('marker: [%s] %d', self.category.typeid, self.id)
end

--[[ RST
.. lua:class:: trail

    A marker trail. Trails are paths that indicate a route to follow.
    properties that a trail does not define will be derived from the properties
    of the category that contains it.
]]--
M.trail = {}

--[[ RST
    .. lua:method:: mapid([value])

        Get or set the MapID of this trail. If ``value`` is present, it is the
        new MapID. Otherwise the current MapID is returned.

        :param integer value: (Optional) The new MapID
        :rtype: integer
        :returns: The current MapID

        .. versionhistory::
            :0.1.0: Added
]]--
function M.trail:mapid(value)
    if value then
        local s = self.category.markerpack.statements.trail.setmapid
        s:reset()
        s:bind(':mapid', value)
        s:bind(':id', self.id)
        s:step()
        s:reset()
    else
        local s = self.category.markerpack.statements.trail.getmapid
        s:reset()
        s:bind(':id', self.id)
        local r = s:step()
        s:reset()
        if r then return r.mapid end
    end
end


--[[ RST
    .. lua:method:: addpoint(x, y, z)

        Append a point to this trail.

        :param float x:
        :param float y:
        :param float z:

        .. versionhistory::
            :0.1.0: Added
]]--
function M.trail:addpoint(x, y, z)
    local s = self.category.markerpack.statements.trail.maxcoordseq
    s:reset()
    s:bind(':id', self.id)
    local r = s:step()
    s:reset()

    local newseq = r.newseq or 1
    --if r and r.newseq~=nil then newseq = r.newseq end

    s = self.category.markerpack.statements.trail.addcoord
    s:bind(':id', self.id)
    s:bind(':seq', newseq)
    s:bind(':x', x)
    s:bind(':y', y)
    s:bind(':z', z)
    s:step()
    s:reset()
end

--[[ RST
    .. lua:method:: pointsiter()

        Return an iterator that returns x,y,z coordinates on each invocation
        until all points in this trail have been returned.

        .. versionhistory::
            :0.1.0: Added
]]--
function M.trail:pointsiter()
    local s = self.category.markerpack.statements.trail.coords
    s:reset()
    s:bind(':id', self.id)

    local iter = function()
        local r = s:step()

        if r then
            return r.x, r.y, r.z
        else
            s:reset()
            return nil
        end
    end

    return iter
end

--[[ RST
    .. lua:method:: delete()

        Delete this trail and all associated properties/coordinates.

        .. danger::
            
            This object will no longer be valid after calling this method.

        .. versionhistory::
            :0.1.0: Added
]]--
function M.trail:delete()
    local s = self.category.markerpack.statements.trail.delete
    s:reset()
    s:bind(':id', self.id)
    s:step()
    s:reset()

    self.db = nil
    self.id = nil
    self.category = nil
end

-- undocumented, this only exists to allow access to properties that are also
-- method names, i.e. mapid
function M.trail:getproperty(key)
    local s = self.category.markerpack.statements.trail.getprop
    s:reset()
    s:bind(':id', self.id)
    s:bind(':property', string.lower(key))

    local r = s:step()
    s:reset()

    if r then return r.value end
    return self.category[key]
end

function M.trail:__index(key)
    if M.trail[key] and type(M.trail[key])=='function' then return M.trail[key] end

    return M.trail.getproperty(self, key)
end

function M.trail:__newindex(key, value)
    if value==nil then
        local s = self.category.markerpack.statements.trail.delprop
        s:reset()
        s:bind(':id', self.id)
        s:bind(':property', string.lower(key))
        s:step()
        s:reset()
    else
        local s = self.category.markerpack.statements.trail.setprop
        s:reset()
        s:bind(':id', self.id)
        s:bind(':property', string.lower(key))
        s:bind(':value', value)
        s:step()
        s:reset()
    end
end

function M.trail:__tostring()
    return string.format('trail: [%s] %d', self.category.typeid, self.id)
end

--[[ RST
.. lua:class:: datafile

    A data file. This can be any binary data, but is usually an image to be used
    for markers or trail textures.

    .. lua:attribute:: markerpack: markerpack

        The :lua:class:`markerpack` this datafile belongs to.

        .. versionhistory::
            :0.1.0: Added
    .. lua:attribute:: path: string

        This datafile's path.

        .. versionhistory::
            :0.1.0: Added
]]--
M.datafile = {}
M.datafile.__index = M.datafile

--[[ RST
    .. lua:method:: data([value])

        Get or set the content of this data file.

        :param string value: (Optional) Binary data.
        :rtype: string

        .. versionhistory::
            :0.1.0: Added
]]--
function M.datafile:data(value)
    if value then
        local s = self.markerpack.statements.datafile.setdata
        s:reset()
        s:bind(':path', self.path)
        s:bind(':data', value, true)
        s:step()
        s:reset()
    else
        local s = self.markerpack.statements.datafile.getdata
        s:reset()
        s:bind(':path', self.path)
        local r = s:step()
        s:reset()

        if r then
            return r.data
        else
            return nil
        end
    end
end

--[[ RST
    .. lua:method:: delete()

        Delete this data file.

        .. danger
            
            This will remove the data file from the markerpack. Any elements
            that reference this datafile will not behave properly.

            This object will not be valid after this call.

        .. versionhistory::
            :0.1.0: Added
]]--
function M.datafile:delete()
    local s = self.markerpack.statements.datafile.delete
    s:reset()
    s:bind(':path', self.path)
    s:step()
    s:reset()

    self.path = nil
    self.db = nil
end

return M
