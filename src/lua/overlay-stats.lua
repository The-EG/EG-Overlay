-- EG-Overlay
-- Copyright (c) 2025 Taylor Talkington
-- SPDX-License-Identifier: MIT

--[[ RST
Overlay Stats
=============

.. overlay:module:: overlay-stats

Overlay Stats shows the system performance statistics for the overlay and can be
used to monitor CPU and memory impacts.
]]--
local ui = require 'ui'
local overlay = require 'overlay'
local overlay_menu = require 'overlay-menu'
local ml = require 'mumble-link'

local utils = require 'utils'

local function lbl(text)
    return ui.text(text, 0xfcba03ff, ui.fonts.monospace)
end

local function val(text)
    return ui.text(text, 0xFFFFFFFF, ui.fonts.monospace)
end

local function sep()
    local s = ui.separator('horizontal')

    return s
end

local statswin = {}
statswin.__index = statswin

function statswin.new()
    local w = {
        settings = overlay.settings('overlay-stats.lua'),

        last_update = 0,
        last_frames = overlay.framecount(),
        last_ml_tick = ml.tick(),

        last_proc_user_time = nil,
        last_proc_kernel_time = nil,
        last_sys_user_time = nil,
        last_sys_kernel_time = nil,

        win = ui.window('EG-Overlay Stats'),
        box = ui.box('vertical'),
        grid = ui.grid(14,2),

        fps_lbl = lbl('Overlay FPS:'),
        fps_val = val('??.??'),

        gw2_fps_lbl = lbl('GW2 FPS:'),
        gw2_fps_val = val('??.??'),

        luamem_lbl = lbl('Lua Memory:'),
        luamem_val = val('?? MiB'),

        privworkingset_lbl = lbl('Private Working Set:'),
        privworkingset_val = val('?? MiB'),

        totalworkingset_lbl = lbl('Total Working Set:'),
        totalworkingset_val = val('?? MiB'),

        peakworkingset_lbl = lbl('Peak Working Set'),
        peakworkingset_val = val('?? MiB'),

        videomem_lbl = lbl('GPU Memory:'),
        videomem_val = val('?? MiB'),

        cpuusage_lbl = lbl('CPU Usage:'),
        cpuusage_val = val('??%'),

        cputime_lbl = lbl('CPU Time:'),
        cputime_val = val('?? seconds'),

        uptime_lbl = lbl('Uptime:'),
        uptime_val = val('?? seconds'),

        overlay_menu_mi = ui.textmenuitem('Overlay Stats', 0xFFFFFFFF, overlay_menu.font),
    }

    w.settings:setdefault('window.x'      , 10)
    w.settings:setdefault('window.y'      , 10)
    w.settings:setdefault('window.width'  , 150)
    w.settings:setdefault('window.height' , 100)
    w.settings:setdefault('window.visible', false)

    w.box:paddingleft(5)
    w.box:paddingright(5)
    w.box:paddingtop(5)
    w.box:paddingbottom(5)
    w.box:pushfront(w.grid, 'start', false)
    w.win:child(w.box)
    w.win:settings(w.settings, 'window')

    w.grid:colspacing(5)
    w.grid:rowspacing(2)

    w.grid:attach(w.fps_lbl, 1, 1, 1, 1, 'start', 'start')
    w.grid:attach(w.fps_val, 1, 2, 1, 1, 'end'  , 'start')

    w.grid:attach(w.gw2_fps_lbl, 2, 1, 1, 1, 'start', 'start')
    w.grid:attach(w.gw2_fps_val, 2, 2, 1, 1, 'end', 'start')

    w.grid:attach(sep(), 3, 1, 1, 2, 'fill', 'middle')

    w.grid:attach(w.luamem_lbl, 4, 1, 1, 1, 'start', 'start')
    w.grid:attach(w.luamem_val, 4, 2, 1, 1, 'end'  , 'start')

    w.grid:attach(sep(), 5, 1, 1, 2, 'fill', 'middle')

    w.grid:attach(w.privworkingset_lbl, 6, 1, 1, 1, 'start', 'start')
    w.grid:attach(w.privworkingset_val, 6, 2, 1, 1, 'end'  , 'start')

    w.grid:attach(w.totalworkingset_lbl, 7, 1, 1, 1, 'start', 'start')
    w.grid:attach(w.totalworkingset_val, 7, 2, 1, 1, 'end'  , 'start')

    w.grid:attach(w.peakworkingset_lbl, 8, 1, 1, 1, 'start', 'start')
    w.grid:attach(w.peakworkingset_val, 8, 2, 1, 1, 'end'  , 'start')

    w.grid:attach(sep(), 9, 1, 1, 2, 'fill', 'middle')

    w.grid:attach(w.videomem_lbl, 10, 1, 1, 1, 'start', 'start')
    w.grid:attach(w.videomem_val, 10, 2, 1, 1, 'end'  , 'start')

    w.grid:attach(sep(), 11, 1, 1, 2, 'fill', 'middle')

    w.grid:attach(w.cpuusage_lbl, 12, 1, 1, 1, 'start', 'start')
    w.grid:attach(w.cpuusage_val, 12, 2, 1, 1, 'end'  , 'start')

    w.grid:attach(w.cputime_lbl, 13, 1, 1, 1, 'start', 'start')
    w.grid:attach(w.cputime_val, 13, 2, 1, 1, 'end'  , 'start')

    w.grid:attach(w.uptime_lbl, 14, 1, 1, 1, 'start', 'start')
    w.grid:attach(w.uptime_val, 14, 2, 1, 1, 'end'  , 'start')

    setmetatable(w, statswin)

    w.overlay_menu_mi:addeventhandler(function() w:onmenuclick() end, 'click-left')

    w.vislble = false

    if w.settings:get('window.visible') then
        w.win:show()
        w.visible = true
        w.overlay_menu_mi:icon(overlay_menu.visible_icon)
    else
        w.overlay_menu_mi:icon(overlay_menu.hidden_icon)
    end

    return w
end

function statswin:onmenuclick()
    if self.settings:get('window.visible') then
        self.win:hide()
        self.visible = false
        self.settings:set('window.visible', false)
        self.overlay_menu_mi:icon(overlay_menu.hidden_icon)
    else
        self.win:show()
        self.settings:set('window.visible', true)
        self.visible = true
        self.overlay_menu_mi:icon(overlay_menu.visible_icon)
    end
end

function statswin:updatememstats()
    local mem = overlay.memusage()
    local videomem = overlay.videomemusage()

    self.luamem_val:text(utils.formatkbytesize(collectgarbage('count')))
    self.privworkingset_val:text(utils.formatkbytesize(mem.privateworkingset / 1024.0))
    self.totalworkingset_val:text(utils.formatkbytesize(mem.workingset / 1024.0))
    self.peakworkingset_val:text(utils.formatkbytesize(mem.peakworkingset / 1024.0))
    self.videomem_val:text(utils.formatkbytesize(videomem / 1024.0))
end

function statswin:updateproctime()
    local proctime = overlay.processtime()

    if self.last_proc_user_time then
        local overlay_time = ((proctime.usertime   - self.last_proc_user_time  ) +
                              (proctime.kerneltime - self.last_proc_kernel_time))
        local sys_time = ((proctime.systemusertime   - self.last_sys_user_time) +
                          (proctime.systemkerneltime - self.last_sys_kernel_time))

        local usage = overlay_time / sys_time

        local total_cpu_time = utils.durationtostring(proctime.usertime + proctime.kerneltime)

        self.cpuusage_val:text(string.format('%.2f %%', usage * 100.0))
        self.cputime_val:text(total_cpu_time)
        self.uptime_val:text(utils.durationtostring(proctime.processtimetotal))
    end

    self.last_proc_user_time = proctime.usertime
    self.last_proc_kernel_time = proctime.kerneltime
    self.last_sys_user_time = proctime.systemusertime
    self.last_sys_kernel_time = proctime.systemkerneltime
end

function statswin:update()
    if not self.visible then return end

    local now = overlay.time()
    local dur = now - self.last_update

    if dur < 1.0 then return end

    local frames = overlay.framecount() - self.last_frames
    local fps = frames / dur
    self.last_frames = overlay.framecount()
    self.last_update = now

    local ml_tick = ml.tick()
    local gw2_frames = ml_tick - self.last_ml_tick
    local gw2_fps = gw2_frames / dur
    self.last_ml_tick = ml_tick

    self.fps_val:text(string.format('%.2f', fps))
    self.gw2_fps_val:text(string.format('%.2f', gw2_fps))

    self:updatememstats()
    self:updateproctime()
end

function statswin:setup()
    overlay_menu.additem(self.overlay_menu_mi)
end

local stats = statswin.new()

overlay.addeventhandler('startup', function() stats:setup() end)
overlay.addeventhandler('update', function() stats:update() end )

return {}
