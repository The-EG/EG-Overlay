--[[ RST
mumble-link-events
==================

.. lua:module:: mumble-link-events

.. code:: lua

    require 'mumble-link-events'

The :lua:mod:`mumble-link-events` module does not have any public functions or
attributes, however once it is loaded from another module using ``require``, it
will send events based on the data accessible from the :lua:mod:`mumble-link`
module.

.. note::
    This module checks particular values in :lua:mod:`mumble-link` on every
    :overlay:event:`update` event. Any events that it queues are dispatched on
    the next :overlay:event:`update`.


Events
------

.. overlay:event:: mumble-link-available

    Sent once :lua:data:`mumble-link.tick` has changed after a
    :overlay:event:`mumble-link-unavailable` event.

    .. versionhistory::
        :0.0.1: Added


.. overlay:event:: mumble-link-unavailable

    Sent anytime :lua:data:`mumble-link.tick` is not updated for at least 400
    milliseconds, indicating that the game is no longer updating the MumbleLink
    data.
    
    Once this event has been sent additional MumbleLink related events will not
    be sent until :overlay:event:`mumble-link-available` is sent again.

    .. versionhistory::
        :0.0.1: Added

.. overlay:event:: mumble-link-map-changed

    Sent anytime :lua:data:`mumble-link.map_id` changes between update events.

    .. versionhistory::
        :0.0.1: Added
]]--

local overlay = require 'eg-overlay'
local logger = require 'logger'
local ml = require 'mumble-link'

local log = logger.logger:new("mumble-link-events")
local is_available = false
local last_tick = ml.tick
local last_tick_time = overlay.time()
local map_id = ml.mapid

local charactername = ml.charactername

local tick_to_tick_time = 0.4

local function update()
    local now = overlay.time()
    if not is_available and last_tick~=ml.tick then
        is_available = true
        overlay.queueevent('mumble-link-available')
        log:debug('MumbleLink available')
    elseif is_available and last_tick==ml.tick and now - last_tick_time >= tick_to_tick_time then
        is_available = false
        overlay.queueevent('mumble-link-unavailable')
        log:debug('MumbleLink unavailable')
    end

    if last_tick ~= ml.tick then
        last_tick = ml.tick
        last_tick_time = now
    end

    if not is_available then return end

    if charactername~=ml.charactername then
        log:debug('MumbleLink character changed ( %s -> %s )', charactername, ml.charactername)
        overlay.queueevent('mumble-link-character-changed', {from=charactername, to=ml.charactername})
        charactername = ml.charactername
    end

    if map_id~=ml.mapid then
        log:debug('MumbleLink new map ( %d -> %d )', map_id, ml.mapid)
        map_id = ml.mapid
        overlay.queueevent('mumble-link-map-changed')
    end
end

overlay.addeventhandler('update', update)

return {}
