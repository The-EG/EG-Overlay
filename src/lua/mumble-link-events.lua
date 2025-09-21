-- EG-Overlay
-- Copyright (c) 2025 Taylor Talkington
-- SPDX-License-Identifier: MIT

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

    Sent once :lua:func:`mumble-link.tick` has changed after a
    :overlay:event:`mumble-link-unavailable` event.

    .. versionhistory::
        :0.3.0: Added

.. overlay:event:: mumble-link-unavailable

    Sent anytime :lua:func:`mumble-link.tick` is not updated for at least 400
    milliseconds, indicating that the game is no longer updating the MumbleLink
    data.
    
    Once this event has been sent additional MumbleLink related events will not
    be sent until :overlay:event:`mumble-link-available` is sent again.

    .. versionhistory::
        :0.3.0: Added

.. overlay:event:: mumble-link-map-changed

    Sent anytime :lua:func:`mumble-link.context.mapid` changes between update events.

    Event handlers will be sent a Lua table containing two fields:

    +-------+-------------------------------------------------------------+
    | Field | Description                                                 |
    +=======+=============================================================+
    | from  | The map ID of the prior map.                                |
    +-------+-------------------------------------------------------------+
    | to    | The map ID of the new map.                                  |
    +-------+-------------------------------------------------------------+

    .. warning::
        ``from`` may be ``0`` if there was not a valid map previously.

    .. versionhistory::
        :0.3.0: Added

.. overlay:event:: mumble-link-character-changed

    Sent anytime :lua:func:`mumble-link.identity.name` changes between update events.

    Event handlers will be sent a Lua table containing two fields:

    +-------+------------------------------------------------------------------+
    | Field | Description                                                      |
    +=======+==================================================================+
    | from  | The previous character name.                                     |
    +-------+------------------------------------------------------------------+
    | to    | The new character name.                                          |
    +-------+------------------------------------------------------------------+

    .. warning::
        ``from`` may be ``nil`` if there was no previous character name.

    .. versionhistory::
        :0.3.0: Added
]]--

local overlay = require 'overlay'
local ml = require 'mumble-link'

local mlevents = {}
mlevents.__index = mlevents

function mlevents.new()
    local m = {
        available = false,
        lasttick = ml.tick(),
        lastticktime = overlay.time(),
        mapid = ml.context.mapid(),
        charactername = ml.identity.name(),
    }

    setmetatable(m, mlevents)

    return m
end

function mlevents:checkavailable(now)
    local tick = ml.tick()

    if not self.available and self.lasttick ~= tick then
        self.available = true
        overlay.queueevent('mumble-link-available')
        overlay.logdebug('MumbleLink available')
    elseif self.available and self.lasttick == tick and now - self.lastticktime >= 0.4 then
        self.available = false
        overlay.queueevent('mumble-link-unavailable')
        overlay.logdebug('MumbleLink unavailable.')
    end

    if self.lasttick ~= tick then
        self.lasttick = tick
        self.lastticktime = now
    end
end

function mlevents:checkcharacter(now)
    local charname = ml.identity.name()

    if self.charactername ~= charname then
        overlay.queueevent('mumble-link-character-changed', {from=self.charactername, to=charname})
        overlay.logdebug(string.format('MumbleLink character changed (%s -> %s)',self.charactername, charname))

        self.charactername = charname
    end
end

function mlevents:checkmap(now)
    local mapid = ml.context.mapid()

    if self.mapid ~= mapid then
        overlay.queueevent('mumble-link-map-changed', {from=self.mapid, to=mapid})
        overlay.logdebug(string.format('MumbleLink map changed (%s -> %s)', self.mapid, mapid))

        self.mapid = mapid
    end
end

function mlevents:onupdate()
    local now = overlay.time()

    self:checkavailable(now)

    if not self.available then return end

    self:checkcharacter(now)
    self:checkmap(now)
end

local e = mlevents.new()

overlay.addeventhandler('update', function() e:onupdate() end)

return {}
