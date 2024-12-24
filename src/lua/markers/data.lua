local overlay = require 'eg-overlay'
local sqlite = require 'sqlite'
local logger = require 'logger'

local M = {}

local log = logger.logger:new('markers')

local stmts = {}

local function onstartup()
    local dbpath = overlay.datafolder('markers') .. '/markers.db'

    M.db = sqlite.open(dbpath)

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
    M.db:execute('PRAGMA journal_mode = WAL')
    M.db:execute('PRAGMA synchronous = NORMAL')

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
          AND (activateid = :activateid OR
            (activateid IS NULL and :activateid IS NULL))
    ]])
    stmts.guidactiveday = M.db:prepare([[
        SELECT COUNT(*) AS count
        FROM guidtimes
        WHERE guid = :guid
          AND (
            activateid = :activateid OR
            (activateid IS NULL and :activateid IS NULL))
          AND unixepoch(starttime) > unixepoch('now','start of day')
    ]])
    stmts.guidactiveweek = M.db:prepare([[
        SELECT COUNT(*) AS count
        FROM guidtimes
        WHERE guid = :guid
          AND unixepoch(starttime) > unixepoch('now','start of day','weekday 1','-7 days','+07:30')
          AND (activateid = :activateid OR 
            (activateid IS NULL and :activateid IS NULL)
          )
    ]])
    stmts.activateguid = M.db:prepare([[
        INSERT INTO guidtimes (guid, starttime, activateid)
        VALUES (:guid, datetime(), :activateid)
        ON CONFLICT (guid, activateid) DO
        UPDATE SET starttime = datetime()
        WHERE guid = :guid
          AND (activateid = :activateid OR
            (activateid IS NULL AND :activateid IS NULL)
          )
    ]])
end

function M.dumpguidtimes()
    local s = M.db:prepare('SELECT guid, starttime, activateid FROM guidtimes')

    log:info('Dumping marker GUID times:')
    log:info('GUID                           Time                Activate ID')
    log:info('============================== =================== =============================')

    local rows = function() return s:step() end

    for row in rows do
        log:info("%-30s %19s %s", row.guid, row.starttime, row.activateid or '(none)')
    end

    log:info('------------------------------ ------------------- -----------------------------')
    s:finalize()
end

function M.clearguidtimes(guid)
    local s = M.db:prepare('DELETE FROM guidtimes WHERE guid = :guid OR :guid IS NULL')
    s:bind(':guid', guid)
    s:step()
    s:finalize()
end

function M.iscategoryactive(category, ancestors)
    local s = stmts.selectcat
    s:reset()
    s:bind(':typeid', category.typeid)
    local r = s:step()
    s:reset()

    local a = (category.defaulttoggle or 1)==1
    if r then
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

function M.setcategoryactive(category, active)
    local s = stmts.setcatactive
    s:reset()
    s:bind(':active', active)
    s:bind(':typeid', category.typeid)
    s:step()
    s:reset()
end

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

    if not r then return false end
    return r.count > 0
end

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
