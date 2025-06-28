-- EG-Overlay
-- Copyright (c) 2025 Taylor Talkington
-- SPDX-License-Identifier: MIT

--[[ RST
gw2.data
========

.. lua:module:: gw2.data

.. code-block:: lua

    local gw2data = require 'gw2.data'

The :lua:mod:`gw2.data` module contains cached data from the GW2 API. This is
data that does not require authentication and does not change regularly, such as
map information.

This allows access to this data without querying the API every time.

.. important::
    Module authors should prefer using this module over querying the API for the
    same data.

Functions
---------
]]--

local overlay = require 'eg-overlay'

local M = {}

local datapath = overlay.datafolder('gw2')

M.db = overlay.sqlite3open(string.format('%s\\data.db', datapath))

-- fetch all ids from the endpoint and return all results as a single table
local function fetchallids(endpoint)
    local api = require('gw2').basicapi.new()

    local ids = api:get(endpoint)

    if not ids then
        error(string.format("Couldn't query %s", endpoint))
    end

    overlay.loginfo(string.format('Fetching %d records from %s...', #ids, endpoint))

    local reqs = 0 -- how many requests we've made this second
    local lastbuckettime = overlay.time()

    local prog = 0

    local fetchids = {}

    local allrecords = {}

    while #ids > 0 do
        while #fetchids > 0 do table.remove(fetchids) end

        while #ids > 0 and #fetchids < 200 do table.insert(fetchids, table.remove(ids)) end

        local ids_str = table.concat(fetchids,',')

        local records = api:get(endpoint, { ids = ids_str })

        if not records then
            error("Couldn't query endpoint")
        end

        while #records > 0 do table.insert(allrecords, table.remove(records)) end

        reqs = reqs + 1
        if reqs > 5 then
            while overlay.time() - lastbuckettime <= 1.0 do
                coroutine.yield()
            end
            lastbuckettime = overlay.time()
            reqs = 0
        end

        prog = prog + #fetchids

        overlay.loginfo(string.format('  %d completed.', prog))
    
        coroutine.yield()
    end

    return allrecords
end

--[[ RST
.. lua:function:: map(mapid)

    Return information about the given map id.

    Returns a Lua table with the following fields:

    * id
    * name
    * min_level
    * max_level
    * type
    * default_floor
    * region_id
    * region_name
    * continent_id
    * continent_name
    * map_rect_left
    * map_rect_right
    * map_rect_top
    * map_rect_bottom
    * continent_rect_left
    * continent_rect_right
    * continent_rect_top
    * continent_rect_bottom

    .. seealso::

        `GW2 API: v2/maps <https://wiki.guildwars2.com/wiki/API:2/maps>`_

    :rtype: table

    .. versionhistory::
        :0.3.0: Added
]]--
function M.map(mapid)
    local sel = M.db:prepare([[SELECT * FROM maps WHERE id = :mapid]])

    sel:bind(':mapid', mapid)

    local r = sel:step()

    if r==true then
        overlay.logwarn(string.format('Invalid Map ID: %d', mapid))
        return
    end

    return r
end

local create_map_table_sql = [[
CREATE TABLE IF NOT EXISTS maps (
    id INTEGER PRIMARY KEY NOT NULL,
    name TEXT NOT NULL,
    min_level INTEGER NOT NULL,
    max_level INTEGER NOT NULL,
    default_floor INTEGER NOT NULL,
    map_rect_left REAL NOT NULL,
    map_rect_right REAL NOT NULL,
    map_rect_top REAL NOT NULL,
    map_rect_bottom REAL NOT NULL,
    continent_rect_left REAL NOT NULL,
    continent_rect_right REAL NOT NULL,
    continent_rect_top REAL NOT NULL,
    continent_rect_bottom REAL NOT NULL,
    region_id INTEGER,
    region_name TEXT,
    continent_id INTEGER,
    continent_name TEXT,
    type TEXT NOT NULL
)
]]

local maps_ins_sql = [[
INSERT INTO maps
(id, name, min_level, max_level, default_floor,
 map_rect_left, map_rect_right, map_rect_top, map_rect_bottom,
 continent_rect_left, continent_rect_right, continent_rect_top, continent_rect_bottom,
 region_id, region_name, continent_id, continent_name, type)
VALUES
(:id, :name, :min_level, :max_level, :default_floor,
 :map_rect_left, :map_rect_right, :map_rect_top, :map_rect_bottom,
 :continent_rect_left, :continent_rect_right, :continent_rect_top, :continent_rect_bottom,
 :region_id, :region_name, :continent_id, :continent_name, :type)
]]

if not M.db:execute(create_map_table_sql) then
    error("Couldn't create maps table.")
end

--[[ RST
.. lua:function:: refreshmaps()

    Updates the cached map data from the GW2 API.

    .. versionhistory::
        :0.3.0: Added
]]--
function M.refreshmaps()
    local ins = M.db:prepare(maps_ins_sql)

    local maps = fetchallids('/maps')
    overlay.loginfo("Updating maps table...")

    M.db:execute('BEGIN')
    M.db:execute('DELETE FROM maps')

    for i, map in ipairs(maps) do           
        ins:bind(':id', map.id)
        ins:bind(':name', map.name)
        ins:bind(':min_level', map.min_level)
        ins:bind(':max_level', map.max_level)
        ins:bind(':default_floor', map.default_floor)
        ins:bind(':type', map.type)
        ins:bind(':region_id', map.region_id)
        ins:bind(':region_name', map.region_name)
        ins:bind(':continent_id', map.continent_id)
        ins:bind(':continent_name', map.continent_name)
        ins:bind(':map_rect_left', map.map_rect[1][1])
        ins:bind(':map_rect_right', map.map_rect[2][1])
        ins:bind(':map_rect_top', map.map_rect[2][2])
        ins:bind(':map_rect_bottom', map.map_rect[1][2])
        ins:bind(':continent_rect_left', map.continent_rect[1][1])
        ins:bind(':continent_rect_right', map.continent_rect[2][1])
        ins:bind(':continent_rect_top', map.continent_rect[2][2])
        ins:bind(':continent_rect_bottom', map.continent_rect[1][2])
        if not ins:step() then
            M.db:execute('ROLLBACK')
            error("Couldn't update maps table.")
        end
        ins:reset()
    end

    M.db:execute('COMMIT')

    overlay.loginfo("Maps table refresh complete.")
end

--[[ RST
.. lua:function:: specialization(id)

    Return information about the given specialization ID.

    Returns a Lua table with the following fields:

    * id
    * name
    * profession
    * elite
    * icon
    * background

    .. seealso::

        `GW2 API: v2/specializations <https://wiki.guildwars2.com/wiki/API:2/specializations>`_

    :rtype: table

    .. versionhistory::
        :0.3.0: Added        
]]--
function M.specialization(id)
    local sel = M.db:prepare([[SELECT * FROM specializations WHERE id = :id]])

    sel:bind(':id', id)

    local r = sel:step()

    if r==true then
        overlay.logwarn(string.format('Invalid Specialization ID: %d', id))
        return
    end

    return r
end

local create_specs_table_sql = [[
CREATE TABLE IF NOT EXISTS specializations (
    id INTEGER PRIMARY KEY NOT NULL,
    name TEXT NOT NULL,
    profession TEXT NOT NULL,
    elite INTEGER NOT NULL,
    icon TEXT,
    background TEXT
)
]]

if not M.db:execute(create_specs_table_sql) then
    error("Couldn't create specializations table.")
end

local specs_ins_sql = [[
INSERT INTO specializations
(id, name, profession, elite, icon, background)
VALUES
(:id, :name, :profession, :elite, :icon, :background)
]]

--[[ RST
.. lua:function:: refreshspecializations()

    Updates the cached specialization data from the GW2 API.

    .. versionhistory::
        :0.3.0: Added
]]--
function M.refreshspecializations()
    local ins = M.db:prepare(specs_ins_sql)

    local specs = fetchallids('/specializations')

    overlay.loginfo("Refreshing specializations table:")

    M.db:execute('BEGIN')
    M.db:execute('DELETE FROM specializations')

    for i, spec in ipairs(specs) do           
        ins:bind(':id', spec.id)
        ins:bind(':name', spec.name)
        ins:bind(':profession', spec.profession)
        ins:bind(':elite', spec.elite)
        ins:bind(':icon', spec.icon)
        ins:bind(':background', spec.background)
        if not ins:step() then
            M.db:execute('ROLLBACK')
            error("Couldn't update specializations table.")
        end
        ins:reset()
    end

    M.db:execute('COMMIT')

    overlay.loginfo("Specializations table refresh complete.")
end


return M
