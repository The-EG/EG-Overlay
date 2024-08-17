--[[ RST
gw2.api
=======

.. lua:module:: gw2.api

.. code-block:: lua

    local api = require 'gw2.api'

]]--


local web_request = require 'web-request'
local jansson = require 'jansson'
local settings = require 'settings'
local overlay = require 'eg-overlay'

local logger = require 'logger'

local api = {}

-- cahced results
-- keys are specific endpoints, ie 'achievements?ids=3147'
-- values are a table with time and data fields
api.cache = {}

api.log = logger.logger:new('gw2.api')


--[[ RST
Settings
--------

The settings for :lua:mod:`gw2.api` are stored in ``settings/gw2.api.lua.json``.
]]--
api.settings = settings.new('gw2.api.lua')

--[[ RST
.. confval:: enableCache
    :type: boolean
    :default: true

    Cache results based on request URLs.

    .. versionhistory::
        :0.0.1: Added
]]--
api.settings:set_default('enableCache', true)

--api.settings:set_default('cacheValidTime', 60)

--[[ RST
.. confval:: apiKey
    :type: string
    :default: null

    The api key to use when making requests.
    
    .. versionhistory::
        :0.0.1: Added
]]--
api.settings:set_default('apiKey', nil)


local function api_url(endpoint)
    return string.format('https://api.guildwars2.com/v2/%s', endpoint)
end

local function setup_request(request)
    local key = api.settings:get('apiKey')

    if key then request:add_header('Authorization', 'Bearer ' .. key) end
    request:add_header('X-Schema-Version', 'latest')
end

local function dispatch_json_response(code, data, done, err, endpoint, cache)
    if cache==nil then cache = true end

    if code >= 200 and code < 400 then
        local r,data_json = jansson.loads(data)
        if not r and err then
            err("Couldn't parse response.")
            return
        end
        if api.settings:get('enableCache') and cache then
            api.cache[endpoint] = {
                data = data_json,
                time = overlay.time()
            }
        end
        if done then done(data_json) end
    else
        if err then err(data) end
    end
end

local function client_request(endpoint)
    local req = web_request.new(api_url(endpoint))
    setup_request(req)

    return req
end

local function idstostr(ids)
    if ids==nil then
        return nil
    elseif type(ids)=='table' then
        return table.concat(ids, ',')
    elseif type(ids)=='number' then
        if math.tointeger(ids) then 
            return tostring(math.tointeger(ids)) 
        end
        return tostring(ids)
    elseif type(ids)=='string' then
        return ids
    elseif ids=='all' then
        return 'all'
    else
        error("ids parameter must be a sequence of values, a single value, or 'all'", 3)
    end
end

local function getidsfunc(endpoint, idsname)
    return function(ids, done, err) return api.getwithids(endpoint, ids, idsname, done, err, 4, true) end
end

local function getfunc(endpoint)
    return function(done, err) return api.get(endpoint, nil, done, err, 4, true) end
end

--[[ RST
Functions
---------
]]--


--[[ RST
.. lua:function:: get(endpoint, params[, done[, err] ])

    Perform a request to the given ``endpoint``.

    .. important::
        All requests are performed asynchronously on a separate thread, but if
        ``done`` and ``err`` are not supplied this function will wait on the
        for the response using Lua coroutines and return it when it is
        available.  

        This means that any function that calls this function in that
        manner will become a yielding coroutine.

    :param endpoint: The GW2 API endpoint to request.
    :type endpoint: string
    :param params: The query parameters to append to the URL.
    :type params: table
    :param done: (Optional) A function to call once the request is successfully
        completed.
    :type done: function
    :param err: (Optional) A function to call if an error occurs while completing
        the request.
    :type err: function
    :returns: If ``done`` and ``err`` are nil this function returns either a
        parsed JSON object or an error string. Otherwise ``nil``.

    .. code-block:: lua
        :caption: Example

        local params = {
            ids = "1840,910,2258"
        }

        -- a synchronous request
        local achievements = api.get('achievements', params)

        local function done(achievements)
            -- process JSON data
        end

        local function err(msg)
            -- respond to error
        end

        -- an asynchronous request
        api.get('achievements', params, done, err)

    .. versionhistory::
        :0.0.1: Added
]]--
function api.get(endpoint, params, done, err, sd, cache)
    if cache==nil then cache = true end
    if cache and api.settings:get('enableCache') then
        if api.cache[endpoint] then
            local now = overlay.time()
            if now - api.cache[endpoint].time < api.settings:get('cacheValidTime') then
                local di = debug.getinfo(sd, 'Sl')
                local parts = {}
                for p in string.gmatch(di.short_src, '[^\\]+') do table.insert(parts, p) end
                
                api.log:info('%s:%d: CACHED https://api.guildwars2.com/v2/%s', parts[#parts], di.currentline, endpoint)

                if done then 
                    done(api.cache[endpoint].data)
                else
                    return true, api.cache[endpoint].data
                end
            end
        end
    end

    local req = client_request(endpoint)

    if params then
        for p,v in pairs(params) do
            req:add_query_parameter(p, v)
        end
    end

    if done~=nil or err~=nil then
        req:queue(function(code, data, r)
            dispatch_json_response(code, data, done, err, endpoint, cache)
        end, sd or 2)
    else
        local finished = false
        local success = false
        local ret_data = nil
        done = function(data)
            success = true
            ret_data = data
            finished = true
        end
        err = function(data)
            success = false
            ret_data = data
            finished = true
        end

        req:queue(function(code, data, r)
            dispatch_json_response(code, data, done, err, endpoint, cache)
        end, sd or 2)

        while not finished do
            coroutine.yield()
        end

        return success, ret_data
    end
end

function api.getwithids(endpoint, ids, paramname, done, err, sd, cached)
    local idstr = idstostr(ids)
    paramname = paramname or 'ids'
    local params = {}
    
    if idstr then
        params[paramname] = idstr
    end

    return api.get(endpoint, params, done, err, sd or 3, cached)
end

-- api.achievements = {
--     __call = function(_func, ids, done, err) return api.getwithids('achievements', ids, nil, done, err, 4, true) end,

--     groups     = getidsfunc('achievements/groups'),
--     categories = getidsfunc('achievements/categories')
-- }
-- api.achievements.__index = api.achievements
-- setmetatable(api.achievements, api.achievements)


-- api.account = {
--     __call = function(_func, done, err) return api.get('account', done, err, 4, true) end,

--     achievements    = getfunc('account/achievements'),
--     bank            = getfunc('account/bank'),
--     dailycrafting   = getfunc('account/dailycrafting'),
--     dungeons        = getfunc('account/dungeons'),
--     dyes            = getfunc('account/dyes'),
--     finishers       = getfunc('account/finishers'),
--     gliders         = getfunc('account/gliders'),
--     home = {
--         cats        = getfunc('account/home/cats'),
--         nodes       = getfunc('account/home/nodes')
--     },
--     inventory       = getfunc('account/inventory'),
--     jadebots        = getfunc('account/jadebots'),
--     luck            = getfunc('account/luck'),
--     legendaryarmory = getfunc('account/legendaryarmory'),
--     mailcarriers    = getfunc('account/mailcarriers'),
--     mapchests       = getfunc('account/mapchests'),
--     masteries       = getfunc('account/masteries'),
--     mastery = {
--         points      = getfunc('account/mastery/points'),
--     },
--     materials       = getfunc('account/materials'),
--     minis           = getfunc('account/minis'),
--     mounts = {
--         skins       = getfunc('account/mounts/skins'),
--         types       = getfunc('account/mounts/types')
--     },
--     novelties       = getfunc('account/novelties'),
--     outfits         = getfunc('account/outfits'),
--     progression     = getfunc('account/progression'),
--     pvp = {
--         heroes      = getfunc('account/pvp/heroes')
--     },
--     raids           = getfunc('account/raids'),
--     recipes         = getfunc('account/recipes'),
--     skiffs          = getfunc('account/skiffs'),
--     skins           = getfunc('account/skins'),
--     titles          = getfunc('account/titles'),
--     wallet          = getfunc('account/wallet'),
--     wizardsvault = {
--         daily       = getfunc('account/wizardsvault/daily'),
--         listings    = getfunc('account/wizardsvault/listings'),
--         special     = getfunc('account/wizardsvault/special' ),
--         weekly      = getfunc('account/wizardsvault/weekly')
--     },
--     worldbosses     = getfunc('account/worldbosses')
-- }
-- api.account.__index = api.account
-- setmetatable(api.account, api.account)

-- local charactersfunc(endpoint)
--     return function(id, done, err) return api.get(string.format('characters/%s/%s', id, endpoint), done, err, 4, true) end
-- end

-- api.characters = {
--     __call = return function(_func, ids, done, err) return api.getwithids('characters', ids, nil, done, err, 4, true) end,

--     backstory       = charactersfunc('backstory'),
--     buildtabs       = function(charid, ids, done, err) return api.getwithids(string.format('characters/%s/buildtabs', charid), ids, 'tabs', done, err, 4, true) end,
--     core            = charactersfunc('core'),
--     crafting        = charactersfunc('crafting'),
--     equipment       = charactersfunc('equipment'),
--     equipmenttabs   = function(charid, ids, done, err) return api.getwithids(string.format('characters/%s/equipmenttabs', charid), ids, 'tabs', done, err, 4, true) end,
--     inventory       = charactersfunc('inventory'),
--     recipes         = charactersfunc('recipes'),
--     skills          = charactersfunc('skills'),
--     specializations = charactersfunc('specializations'),
--     training        = charactersfunc('training'),
--     dunegons        = charactersfunc('dungeons'),
--     heropoints      = charactersfunc('heropoints'),
--     quests          = charactersfunc('quests'),
--     sab             = charactersfunc('sab')
-- }
-- api.characters.__index = api.characters
-- setmetatable(api.characters, api.characters)


-- api.maps = function(ids, done, err) return api.getwithids('maps', ids, nil, done, err, 4, true) end



return api