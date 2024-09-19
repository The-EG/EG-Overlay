--[[ RST
Overlay Stats
=============

.. image:: /images/modules/overlay-stats.png

.. overlay:module:: overlay-stats

Overlay Stats shows the system stats for the overlay and can be used to monitor
performance or memory impacts.
]]--

local ui = require 'eg-overlay-ui'
local uih = require 'ui-helpers'
local overlay = require 'eg-overlay'
local sqlite = require 'sqlite'
local settings = require 'settings'

local overlay_stats = {}

local function text_label(label)
    return uih.monospace_text(label, 0xfcba03ff)
end

local function text_value(value)
    return uih.monospace_text(value, 0xFFFFFFFF)
end

local function duration_to_string(milliseconds)
    local milliseconds_bal = milliseconds % 1000.0
    local hours = math.tointeger(math.floor(milliseconds / 1000.0 / 60.0 / 60.0))
    local minutes = math.tointeger(math.floor(milliseconds / 1000.0 / 60.0)) - (hours * 60)
    local seconds = math.tointeger(math.floor(milliseconds / 1000.0)) - (hours * 60 * 60) - (minutes * 60)

    local str = string.format('%d.%03.0f s', seconds, milliseconds_bal)
    if minutes > 0 then str = string.format('%d m %s', minutes, str) end
    if hours > 0 then str = string.format('%d h %s', hours, str) end

    return str
end

local os_settings = settings.new('overlay-stats.lua')
os_settings:set_default('window.x', 10)
os_settings:set_default('window.y', 10)
os_settings:set_default('window.width', 150)
os_settings:set_default('window.height', 100)

overlay_stats.win = ui.window("EG-Overlay Stats", 10, 10)
overlay_stats.win:settings(os_settings, 'window')

local outer_box = ui.box('vertical')
outer_box:padding(5,5,5,5)

local function stat_line(label, initval)
    local group = {
        label = text_label(string.format('%s: ', label)),
        value = text_value(initval),
        box = ui.box('horizontal')
    }
    group.box:pack_end(group.label, true, 'start')
    group.box:pack_end(group.value, false, 'end')
    outer_box:pack_end(group.box, false,'fill')

    return group
end

local function seperator()
    local sep = ui.separator('horizontal')
    outer_box:pack_end(sep, false, 'fill')
end

local fps = stat_line('FPS', '???')

seperator()

local sqlite_mem = stat_line('SQLite Memory', '?? MB')

local lua_mem = stat_line('Lua Memory', '?? MB')

seperator()

local mem_cur = stat_line('Total Memory', '?? MB')
local mem_peak = stat_line('Peak Memory', '?? MB')

seperator()

local cpu_usage = stat_line('CPU Usage', '??%')
local cpu_time = stat_line('CPU Time', '?? seconds')

local uptime = stat_line('Overlay Uptime', '?? seconds')

local last_process_user_time = nil
local last_process_kernel_time = nil
local last_system_user_time = nil
local last_system_kernel_time = nil
local last_process_time = nil

overlay_stats.win:set_child(outer_box)

local last_update = overlay.time()
local frames = 0

overlay_stats.win:show()

function overlay_stats.update()
    frames = frames + 1
    local now = overlay.time()

    local dur = now - last_update

    if dur < 0.5 then return end

    local cur_fps = frames / dur
    frames = 0
    last_update = now

    fps.value:update_text(string.format('%.2f', cur_fps))

    local mem = overlay.mem_usage()
    mem_cur.value:update_text(string.format('%.2f MB', mem.working_set / 1024.0 / 1024.0))
    mem_peak.value:update_text(string.format('%.2f MB', mem.peak_working_set / 1024.0 / 1024.0))

    sqlite_mem.value:update_text(string.format('%.2f MB', sqlite.memory_used() / 1024.0 / 1024.0))

    lua_mem.value:update_text(string.format('%.2f MB', collectgarbage('count') / 1024.0))

    local proc_time = overlay.process_time()
    local now = overlay.time()

    if last_process_time then
        local overlay_time = ((proc_time.user_time - last_process_user_time) + 
                              (proc_time.kernel_time - last_process_kernel_time))
        local sys_time = ((proc_time.system_user_time - last_system_user_time) +
                          (proc_time.system_kernel_time - last_system_kernel_time))
        
        local usage = overlay_time / sys_time

        local total_cpu_time = duration_to_string(proc_time.user_time + proc_time.kernel_time)

        cpu_usage.value:update_text(string.format('%.2f%%', usage * 100.0))
        cpu_time.value:update_text(total_cpu_time)
        uptime.value:update_text(duration_to_string(proc_time.process_time_total))
    end
    last_process_time = now
    last_process_user_time = proc_time.user_time
    last_process_kernel_time = proc_time.kernel_time
    last_system_user_time = proc_time.system_user_time
    last_system_kernel_time = proc_time.system_kernel_time
end

overlay.add_event_handler('update', overlay_stats.update)

return overlay_stats
