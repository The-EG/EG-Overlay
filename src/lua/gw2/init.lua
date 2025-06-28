-- EG-Overlay
-- Copyright (c) 2025 Taylor Talkington
-- SPDX-License-Identifier: MIT

--[[ RST
gw2
===

.. lua:module:: gw2

Guild Wars 2 utilities, data, and API access.

.. toctree::
    :caption: Submodules
    :maxdepth: 1

    data
]]--


local overlay = require 'eg-overlay'
local ml = require 'mumble-link'

local M = {}

--[[ RST
Functions
---------

.. lua:function:: currentspecialization()

    Return the information on the player's current (elite) specialization.

    This uses MumbleLink to determine what specialization is in the player's
    third build slot.

    See :lua:func:`gw2.data.specialization` and 
    `GW2 API: v2/specializations <https://wiki.guildwars2.com/wiki/API:2/specializations>`_

    .. note::
        This function will return ``nil`` if the MumbleLink data is invalid,
        such as before a character has been logged in for the first time.

    :rtype: table

    .. versionhistory::
        :0.3.0: Added
]]--
function M.currentspecialization()
    local spec = ml.identity.spec()

    if not spec then return nil end

    return require('gw2.data').specialization(spec)
end


--[[ RST
Classes
-------


.. lua:class:: basicapi

    A class that provides basic API access.

    This is an intermediate level wrapper, but does not rely upon particular
    schemas or features of the API to function.

    .. code-block:: lua
        :caption: Example

        local gw2 = require 'gw2'
        local overlay = require 'eg-overlay'

        local api = gw2.basicapi.new()

        -- by default requests are not authenticated
        local build = api:get('/build')
        overlay.loginfo(string.format("GW2 Build ID: %d", build.id))

        -- user can specify an API key
        api:useauth('XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXXXXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX')

        local acount = api:get('/account')
        overlay.loginfo(string.format('Account ID = %s', account.id))

]]--
M.basicapi = {}
M.basicapi.__index = M.basicapi


--[[ RST
    .. lua:function:: new()

        Create a new :lua:class:`basicapi` object.

        :rtype: basicapi

        .. versionhistory::
            :0.3.0: Added
]]--
function M.basicapi.new()
    local a = {
        baseurl = 'https://api.guildwars2.com/v2',
        _schema_version = 'latest',
        _auth_key = nil,
    }

    setmetatable(a, M.basicapi)

    return a
end

--[[ RST
    .. lua:method:: get(endpoint[, params])

        Perform a GET against ``endpoint`` and return the JSON results as a Lua
        table.

        .. warning::
            This function will ``coroutine.yield`` while the request is pending.

        :param string endpoint: Any valid GW2 v2 API endpoint, such as
            ``'/account'``. See `GW2 API <https://wiki.guildwars2.com/wiki/API:Main>`_.
        :returns: A table or ``nil`` on failure, HTTP status code, HTTP response headers

        .. versionhistory::
            :0.3.0: Added
]]--
function M.basicapi:get(endpoint, params)
    local finished = false
    local resp_data = {}
    local resp_hdrs = {}
    local resp_code = 0

    local cb = function(data)
        finished = true
        resp_code = data.status
        resp_hdrs = data.headers
        if data.status >= 200 and data.status < 400 then
            resp_data = overlay.parsejson(data.body)
        else
            resp_data = nil
        end
    end

    local hdrs = {}
    local query_params = params or {}

    query_params.v = self._schema_version

    if self._auth_key then
        hdrs['Authorization'] = string.format('Bearer %s', self._auth_key)
    end

    overlay.webrequest(string.format('%s%s', self.baseurl, endpoint), hdrs, query_params, cb)

    while not finished do coroutine.yield() end

    return resp_data, resp_code, resp_hdrs
end

--[[ RST
    .. lua:method:: useauth(key)

        Sets the API authentication key to use when performing API requests.

        By default no key is used. If ``nil`` is specified, no key will be used.

        :param string key: A valid GW2 API Key or ``nil``.
        
        .. versionhistory::
            :0.3.0: Added
]]--
function M.basicapi:useauth(key)
    self._auth_key = key
end

--[[ RST
.. lua:class:: coordinateconverter

]]--
M.coordinateconverter = {}
M.coordinateconverter.__index = M.coordinateconverter

--[[ RST
    .. lua:function:: new([mapid])

        Create a new coordinate converter for the given map.

        If ``mapid`` is omitted or nil, the new converter will be created for
        the current map, based on mumble-link.

        :rtype: coordinateconverter

        .. versionhistory::
            :0.3.0: Added
]]--
function M.coordinateconverter.new(mapid)
    mapid = mapid or ml.context.mapid()

    if not mapid or mapid==0 then
        error("Can't create coordinateconverter with invalid mapid", 2)
    end

    local map = require('gw2.data').map(mapid)

    local cc = {
        mapid = mapid,
        map = map,
    }

    setmetatable(cc, M.coordinateconverter)

    return cc
end

--[[ RST
    .. lua:method:: map2continent(mapx, mapz)

        Convert map x, z to continent continent x, y coordinates.

        .. note::
            
            This needs the X and Z coordinates from the map coordinates, NOT
            X and Y.

        :returns: 2 numbers

        .. versionhistory::
            :0.3.0: Added
]]--
function M.coordinateconverter:map2continent(mapx, mapz)
    local contleft   = self.map.continent_rect_left
    local contright  = self.map.continent_rect_right
    local conttop    = self.map.continent_rect_top
    local contbottom = self.map.continent_rect_bottom

    local mapleft   = self.map.map_rect_left
    local mapright  = self.map.map_rect_right
    local maptop    = self.map.map_rect_top
    local mapbottom = self.map.map_rect_bottom

    local cx = (contleft   + ( 1 * (mapx - mapleft) / (mapright - mapleft  ) * (contright - contleft  )))
    local cy = (contbottom + (-1 * (mapz - maptop ) / (maptop   - mapbottom) * (conttop   - contbottom)))

    return cx, cy
end

return M
