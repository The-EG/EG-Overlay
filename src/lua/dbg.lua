local logger = require 'logger'

local dbg = {}

dbg.log = logger.logger:new('dbg')

dbg.coroutines = {}

function dbg.continue(co)
    co = co or coroutine.running
    if dbg.coroutines[co]==nil then
        dbg.log:error("Coroutine %s is not being debugged.", tostring(co))
        return
    end

    dbg.coroutines[co].continue = true
end

function dbg.breakpoint(expression)
    expression = expression or true
    local co = coroutine.running()

    if dbg.coroutines[co]==nil then
        dbg.coroutines[co] = {
            continue = true
        }
    end

    local i = 2
    dbg.coroutines[co].info = debug.getinfo(i)

    local info = dbg.coroutines[co].info
    dbg.log:debug("============================================================")
    dbg.log:debug("Debugger breakpoint triggered")
    dbg.log:debug("at %s:%d in %s '%s'", info.short_src, info.currentline, info.namewhat, info.name)
    dbg.log:debug("------------------------------------------------------------")
    
    dbg.log:debug("  Stack trace:")
    while info do
        if info.name then dbg.log:debug("    %s:%d: in %s '%s'", info.short_src, info.currentline, info.namewhat, info.name)
        else dbg.log:debug("    %s:%d", info.short_src, info.currentline) end
        i = i + 1
        info = debug.getinfo(i)
    end
    dbg.log:debug("------------------------------------------------------------")

    dbg.log:debug("  Locals:")
    for l=1, 99999, 1 do
        local name, val = debug.getlocal(2, l)
        if name==nil then break end
        dbg.log:debug("    %s : %s", name, tostring(val))
    end

    dbg.log:debug("============================================================")

    dbg.coroutines[co].continue = false
    
    while not dbg.coroutines[co].continue do
        coroutine.yield()
    end
end

return dbg