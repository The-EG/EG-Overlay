-- EG-Overlay
-- Copyright (c) 2025 Taylor Talkington
-- SPDX-License-Identifier: MIT

--[[ RST
markers.data
============

.. lua:module:: markers.data


]]--
local overlay = require 'overlay'

local M = {}

local stmts = {}

local function onstartup()
    local dbpath = overlay.datafolder('markers') .. '/markers.db'

    M.db = overlay.sqlite3open(dbpath)

    M.db:execute([[
        CREATE TABLE IF NOT EXISTS categories (
            typeid TEXT PRIMARY KEY NOT NULL,
            active BOOL NOT NULL DEFAULT FALSE
        ) WITHOUT ROWID
    ]])

    M.db:execute([[
        CREATE TABLE IF NOT EXISTS guidtimes (
            guid TEXT NOT NULL,
            starttime TEXT NOT NULL,
            activateid TEXT,
            PRIMARY KEY (guid, activateid)
        )
    ]])

    M.db:execute('PRAGMA foreign_keys = ON')
    M.db:execute('PRAGMA optimize=0x10002')
    --M.db:execute('PRAGMA journal_mode = WAL')
    --M.db:execute('PRAGMA synchronous = NORMAL')

    stmts.selectcat = M.db:prepare([[ SELECT typeid, active FROM categories WHERE typeid = :typeid ]])
    stmts.setcatactive = M.db:prepare([[
        INSERT INTO categories (typeid, active)
        VALUES (:typeid, :active)
        ON CONFLICT (typeid) DO UPDATE SET active = :active WHERE typeid = :typeid
    ]])
    stmts.guidactiveperm = M.db:prepare([[
        SELECT COUNT(*) AS count
        FROM guidtimes
        WHERE guid = :guid
          AND (activateid = :activateid OR :activateid IS NULL)
    ]])
    stmts.guidactiveday = M.db:prepare([[
        SELECT COUNT(*) AS count
        FROM guidtimes
        WHERE guid = :guid
          AND (activateid = :activateid OR :activateid IS NULL)
          AND unixepoch(starttime) > unixepoch('now','start of day')
    ]])
    stmts.guidactiveweek = M.db:prepare([[
        SELECT COUNT(*) AS count
        FROM guidtimes
        WHERE guid = :guid
          AND unixepoch(starttime) > unixepoch('now','start of day','weekday 1','-7 days','+07:30')
          AND (activateid = :activateid OR :activateid IS NULL)
    ]])
    stmts.activateguid = M.db:prepare([[
        INSERT INTO guidtimes (guid, starttime, activateid)
        VALUES (:guid, datetime(), :activateid)
        ON CONFLICT (guid, activateid) DO
        UPDATE SET starttime = datetime()
        WHERE guid = :guid
          AND (activateid = :activateid OR :activateid IS NULL)
    ]])
end

--[[ RST
.. lua:function:: dumpguidtimes()

    Print a table of the GUID activation times to the log.

    .. versionhistory::
        :0.3.0: Added
]]--
function M.dumpguidtimes()
    local s = M.db:prepare('SELECT guid, starttime, activateid FROM guidtimes')

    overlay.loginfo('Dumping marker GUID times:')
    overlay.loginfo('GUID                           Time                Activate ID')
    overlay.loginfo('============================== =================== =============================')

    local rows = function() return s:step() end

    for row in rows do
        overlay.loginfo(string.format("%-30s %19s %s", row.guid, row.starttime, row.activateid or '(none)'))
    end

    overlay.loginfo('------------------------------ ------------------- -----------------------------')
end

--[[ RST
.. lua:function:: clearguidtimes(guid)

    Clear all activations from the given GUID.

    :param string guid:

    .. versionhistory::
        :0.3.0: Added
]]--
function M.clearguidtimes(guid)
    local s = M.db:prepare('DELETE FROM guidtimes WHERE guid = :guid OR :guid IS NULL')
    s:bind(':guid', guid)
    s:step()
end

--[[ RST
.. lua:function:: iscategoryactive(category[, ancestors])

    Return ``true`` if the category is active and should be shown or ``false``
    otherwise.

    :param markers.package.category category:
    :param boolean ancestors: (Optional) If the ancestors of ``category`` must also be active.
    :rtype: boolean

    .. versionhistory::
        :0.3.0: Added
]]--
function M.iscategoryactive(category, ancestors)
    local s = stmts.selectcat
    s:reset()
    s:bind(':typeid', category.typeid)
    local r = s:step()
    s:reset()

    local a = (category.defaulttoggle or 1)==1
    if r and type(r)=='table' then
        a = r.active==1
    end

    if a then
        if ancestors then
            local parent = category:parent()
            if parent then
                return M.iscategoryactive(parent, true)
            end
        end
        return true
    else
        return false
    end
end

--[[ RST
.. lua:function:: setcategoryactive(category, active)

    Sets the category to active (to be drawn).

    :param markers.package.category category:
    :param boolean active:

    .. versionhistory::
        :0.3.0: Added
]]--
function M.setcategoryactive(category, active)
    local s = stmts.setcatactive
    s:reset()
    s:bind(':active', active)
    s:bind(':typeid', category.typeid)
    s:step()
    s:reset()
end

--[[ RST
.. lua:function:: guidactive(guid, period[, activateid])

    Return ``true`` if the given guid has already been activated within the
    given ``period`` and ``activateid``.

    :param string guid:
    :param string period: Must be ``'day'``, ``'week'`` or ``'permanent'``.
    :param string activateid: (Optional)

    .. versionhistory::
        :0.3.0: Added
]]--
function M.guidactive(guid, period, activateid)
    if period~='day' and period~='week' and period~='permanent' then
        error("period must be 'day', 'week', or 'permanent'", 2)
    end

    local s

    if period=='week' then
        s = stmts.guidactiveweek
    elseif period=='permanent' then
        s = stmts.guidactiveperm
    elseif period=='day' then
        s = stmts.guidactiveday
    end

    s:reset()
    s:bind(':guid'       , guid)
    s:bind(':activateid' , activateid)

    local r = s:step()
    s:reset()

    if not r or type(r)~='table' then return false end
    return r.count > 0
end

--[[ RST
.. lua:function:: activateguid(guid[, activateid])

    Activate a guid, with an optional activate ID.

    :param string guid:
    :param string activateid: (Optional)

    .. versionhistory::
        :0.3.0: Added
]]--
function M.activateguid(guid, activateid)
    local s = stmts.activateguid
    s:reset()
    s:bind(':guid'     , guid)
    s:bind(':activateid', activateid)
    s:step()
    s:reset()
end

overlay.addeventhandler('startup', onstartup)

return M
