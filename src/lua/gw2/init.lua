--[[ RST
gw2
===

.. lua:module:: gw2

.. code-block:: lua

    local gw2 = require 'gw2'

.. toctree::
    :maxdepth: 1
    :caption: Submodules

    api
    static

]]--


local logger = require 'logger'
local static = require 'gw2.static'
local ml = require 'mumble-link'

local gw2 = {}

gw2.log = logger.logger:new('gw2')


--[[ RST
Functions
---------
]]--


--[[ RST
.. lua:function:: coord_m2c(x, z, contleft, contright, conttop, contbottom, mapleft, mapright, maptop, mapbottom)
    
    Convert a position from map coordinates to continent coordinates.

    :param x: The map X coordinate
    :type x: number
    :param z: The map Z/Y coordinate. Note: this is the 3rd coordinate returned by mumble-link!
    :type z: number
    :param contleft: The left value of the continent_rect from the GW2 API for the map.
    :type contleft: number
    :param contright: The right value of the continent_rect from the GW2 API for the map.
    :type contright: number
    :param conttop: The top value of the continent_rect from the GW2 API for the map.
    :type conttop: number
    :param contbottom: The bottom value of the continent_rect from the GW2 API for the map.
    :type contbottom: number
    :param mapleft: The left value of the map_rect from the GW2 API for the map.
    :type mapleft: number
    :param mapright: The right value of the map_rect from the GW2 API for the map.
    :type mapright: number
    :param maptop: The top value of the map_rect from the GW2 API for the map.
    :type maptop: number
    :param mapbottom: The bottom value of the map_rect from the GW2 API for the map.
    :type mapbottom: number
    :returns: The continent coordinates, as two numbers: cx, cy
    :rtype: number

    .. versionhistory::
        :0.0.1: Added
]]--
function gw2.coord_m2c(x, z, contleft, contright, conttop, contbottom, mapleft, mapright, maptop, mapbottom)
    local cx = (contleft   + ( 1 * (x - mapleft) / (mapright - mapleft  ) * (contright - contleft  )))
    local cy = (contbottom + (-1 * (z - maptop ) / (maptop   - mapbottom) * (conttop   - contbottom)))
    return cx, cy
end

--[[ RST
.. lua:function:: coord_c2m(cx, cy, contleft, contright, conttop, contbottom, mapleft, mapright, maptop, mapbottom)

    Convert a position from continent coordinates to map coordinates.

    :param cx: The continent X coordinate
    :type cx: number
    :param cy: The continent Y coordinate.
    :type cy: number
    :param contleft: The left value of the continent_rect from the GW2 API for the map.
    :type contleft: number
    :param contright: The right value of the continent_rect from the GW2 API for the map.
    :type contright: number
    :param conttop: The top value of the continent_rect from the GW2 API for the map.
    :type conttop: number
    :param contbottom: The bottom value of the continent_rect from the GW2 API for the map.
    :type contbottom: number
    :param mapleft: The left value of the map_rect from the GW2 API for the map.
    :type mapleft: number
    :param mapright: The right value of the map_rect from the GW2 API for the map.
    :type mapright: number
    :param maptop: The top value of the map_rect from the GW2 API for the map.
    :type maptop: number
    :param mapbottom: The bottom value of the map_rect from the GW2 API for the map.
    :type mapbottom: number
    :returns: The map coordinates, as two numbers: x, y. NOTE: the y coordinate corresponds to the third coordinate returned by mumble-link!
    :rtype: number

    .. versionhistory::
        :0.0.1: Added
]]--
function gw2.coord_c2m(cx, cy, contleft, contright, conttop, contbottom, mapleft, mapright, maptop, mapbottom)
    local mx = (mapleft   + ( 1 * (cx - contleft) / (contright - contleft  ) * (mapright - mapright )))
    local mz = (mapbottom + (-1 * (cy - contop  ) / (conttop   - contbottom) * (maptop   - mapbottom)))
    return mx, mz
end

--[[ RST
.. lua:function:: player_map_coords()

    Returns the current player position in map coordinates. This is the value reported by mumble-link converted to inches.

    :returns: X,Y,Z coordinates
    :rtype: number

    **Example**

    .. code-block:: lua

        local px, py, pz = gw2.player_map_coords()

    .. versionhistory::
        :0.0.1: Added
]]--
function gw2.player_map_coords()
    if ml.mapid==0 then return end

    local apos = ml.avatarposition

    return apos.x * 39.3701, apos.y * 39.3701, apos.z * 39.3701
end

--[[ RST
.. lua:function:: player_continent_coords()

    Returns the current player position in continent coordinates. This is based on the data returned by mumble-link and map data from :lua:mod:`gw2.static`.

    :returns: X, Y coordinates
    :rtype: number

    **Example**

    .. code-block:: lua

        local cx, cy = gw2.player_continent_coords()

    .. versionhistory::
        :0.0.1: Added
]]--
function gw2.player_continent_coords()
    if ml.mapid == 0 then return end

    local px, py, pz = gw2.player_map_coords()

    if not px then return end

    local map = static.map(ml.mapid)
    return gw2.coord_m2c(
        px, pz,
        map.continent_rect_left, map.continent_rect_right, map.continent_rect_top, map.continent_rect_bottom,
        map.map_rect_left, map.map_rect_right, map.map_rect_top, map.map_rect_bottom
    )
end

function gw2.map2continent(mapx, mapy)
    if ml.mapid == 0 then return end

    local map = static.map(ml.mapid)

    return gw2.coord_m2c(
        mapx, mapy,
        map.continent_rect_left, map.continent_rect_right, map.continent_rect_top, map.continent_rect_bottom,
        map.map_rect_left, map.map_rect_right, map.map_rect_top, map.map_rect_bottom
    )
end

gw2.coordconverter = {}
gw2.coordconverter.__index = gw2.coordconverter

function gw2.coordconverter:new()
    if ml.mapid == 0 then return end

    local map = static.map(ml.mapid)

    if not map then return end

    local o = { map = map, mapid = ml.mapid }
    setmetatable(o, self)

    return o
end

function gw2.coordconverter:map2continent(mapx, mapz)
    return gw2.coord_m2c(
        mapx, mapz,
        self.map.continent_rect_left, self.map.continent_rect_right,
        self.map.continent_rect_top, self.map.continent_rect_bottom,
        self.map.map_rect_left, self.map.map_rect_right,
        self.map.map_rect_top, self.map.map_rect_bottom
    )
end

return gw2
