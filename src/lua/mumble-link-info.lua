-- EG-Overlay
-- Copyright (c) 2025 Taylor Talkington
-- SPDX-License-Identifier: MIT

--[[ RST
Mumble Link Info
================

.. overlay:module:: mumble-link-info

Mumble Link Info displays the information available from the GW2 MumbleLink
shared memory.
]]--

local overlay = require 'eg-overlay'
local ui = require 'eg-overlay-ui'
local ml = require 'mumble-link'

local M = {}

local function lbltxt(label)
    return ui.text(label, ui.color('accentText'), ui.fonts.regular)
end

local function valtxt(val)
    return ui.text(val, ui.color('text'), ui.fonts.monospace)
end

M.settings = overlay.settings('mumble-link-info.lua')
M.settings:setdefault('window.x', 10)
M.settings:setdefault('window.y', 10)
M.settings:setdefault('window.width', 200)
M.settings:setdefault('window.height', 400)
M.settings:setdefault('window.visible', false)

M.win = ui.window('Mumble Link Info')
M.win:settings(M.settings, 'window')
M.win:resizable(true)

M.scroll = ui.scrollview()
M.win:child(M.scroll)

M.outerbox = ui.box('vertical')
M.outerbox:paddingleft(5)
M.outerbox:paddingright(5)
M.outerbox:paddingtop(5)
M.outerbox:paddingbottom(5)
M.outerbox:spacing(2)
M.scroll:child(M.outerbox)

M.maingrid = ui.grid(9, 2)
M.maingrid:colspacing(4)
M.maingrid:rowspacing(2)
M.outerbox:pushback(M.maingrid, 'fill', false)

M.ml_name = { lbl = lbltxt('Name:'), val = valtxt('(name)') }
M.maingrid:attach(M.ml_name.lbl, 1, 1, 1, 1, 'end'  , 'middle')
M.maingrid:attach(M.ml_name.val, 1, 2, 1, 1, 'start', 'middle')

M.ml_version = { lbl = lbltxt('Version:'), val = valtxt('(version)') }
M.maingrid:attach(M.ml_version.lbl, 2, 1, 1, 1, 'end'  , 'middle')
M.maingrid:attach(M.ml_version.val, 2, 2, 1, 1, 'start', 'middle')

M.ml_tick = { lbl = lbltxt('Tick:'), val = valtxt('(tick)') }
M.maingrid:attach(M.ml_tick.lbl, 3, 1, 1, 1, 'end'  , 'middle')
M.maingrid:attach(M.ml_tick.val, 3, 2, 1, 1, 'start', 'middle')

M.ml_apos = { lbl = lbltxt('Avatar Position:'), val = valtxt('(x, y, z)') }
M.maingrid:attach(M.ml_apos.lbl, 4, 1, 1, 1, 'end'  , 'middle')
M.maingrid:attach(M.ml_apos.val, 4, 2, 1, 1, 'start', 'middle')

M.ml_afront = { lbl = lbltxt('Avatar Front:'), val = valtxt('(x, y, z)') }
M.maingrid:attach(M.ml_afront.lbl, 5, 1, 1, 1, 'end'  , 'middle')
M.maingrid:attach(M.ml_afront.val, 5, 2, 1, 1, 'start', 'middle')

M.ml_atop = { lbl = lbltxt('Avatar Top'), val = valtxt('(x, y, z)') }
M.maingrid:attach(M.ml_atop.lbl, 6, 1, 1, 1, 'end'  , 'middle')
M.maingrid:attach(M.ml_atop.val, 6, 2, 1, 1, 'start', 'middle')

M.ml_cpos = { lbl = lbltxt('Camera Position:'), val = valtxt('(x, y, z)') }
M.maingrid:attach(M.ml_cpos.lbl, 7, 1, 1, 1, 'end'  , 'middle')
M.maingrid:attach(M.ml_cpos.val, 7, 2, 1, 1, 'start', 'middle')

M.ml_cfront = { lbl = lbltxt('Camera Front:'), val = valtxt('(x, y, z)') }
M.maingrid:attach(M.ml_cfront.lbl, 8, 1, 1, 1, 'end'  , 'middle')
M.maingrid:attach(M.ml_cfront.val, 8, 2, 1, 1, 'start', 'middle')

M.ml_ctop = { lbl = lbltxt('Camera Top:'), val = valtxt('(x, y, z)') }
M.maingrid:attach(M.ml_ctop.lbl, 9, 1, 1, 1, 'end'  , 'middle')
M.maingrid:attach(M.ml_ctop.val, 9, 2, 1, 1, 'start', 'middle')

M.outerbox:pushback(ui.separator('horizontal'), 'fill', false)
M.outerbox:pushback(lbltxt('Identity'), 'middle', false)
M.identity = {
    grid    = ui.grid(10, 2),
    name    = { lbl = lbltxt('Name:')          , val = valtxt('(name)') },
    race    = { lbl = lbltxt('Race:')          , val = valtxt('(race)') },
    prof    = { lbl = lbltxt('Profession:')    , val = valtxt('(profession)') },
    spec    = { lbl = lbltxt('Specialization:'), val = valtxt('(specialization)') },
    mapid   = { lbl = lbltxt('Map ID:')        , val = valtxt('(mapid)') },
    worldid = { lbl = lbltxt('World ID:')      , val = valtxt('(worldid)') },
    teamcol = { lbl = lbltxt('Team Color ID:') , val = valtxt('(teamcolorid)') },
    com     = { lbl = lbltxt('Commander:')     , val = valtxt('(commander)') },
    fov     = { lbl = lbltxt('Field of View:') , val = valtxt('(fov)') },
    uisz    = { lbl = lbltxt('UI Size:')       , val = valtxt('(uisz)') },
}

M.outerbox:pushback(M.identity.grid, 'fill', false)
M.identity.grid:colspacing(4)
M.identity.grid:rowspacing(2)

M.identity.grid:attach(M.identity.name.lbl, 1, 1, 1, 1, 'end'  , 'middle')
M.identity.grid:attach(M.identity.name.val, 1, 2, 1, 1, 'start', 'middle')

M.identity.grid:attach(M.identity.race.lbl, 2, 1, 1, 1, 'end'  , 'middle')
M.identity.grid:attach(M.identity.race.val, 2, 2, 1, 1, 'start', 'middle')

M.identity.grid:attach(M.identity.prof.lbl, 3, 1, 1, 1, 'end'  , 'middle')
M.identity.grid:attach(M.identity.prof.val, 3, 2, 1, 1, 'start', 'middle')

M.identity.grid:attach(M.identity.spec.lbl, 4, 1, 1, 1, 'end'  , 'middle')
M.identity.grid:attach(M.identity.spec.val, 4, 2, 1, 1, 'start', 'middle')

M.identity.grid:attach(M.identity.mapid.lbl, 5, 1, 1, 1, 'end'  , 'middle')
M.identity.grid:attach(M.identity.mapid.val, 5, 2, 1, 1, 'start', 'middle')

M.identity.grid:attach(M.identity.worldid.lbl, 6, 1, 1, 1, 'end'  , 'middle')
M.identity.grid:attach(M.identity.worldid.val, 6, 2, 1, 1, 'start', 'middle')

M.identity.grid:attach(M.identity.teamcol.lbl, 7, 1, 1, 1, 'end'  , 'middle')
M.identity.grid:attach(M.identity.teamcol.val, 7, 2, 1, 1, 'start', 'middle')

M.identity.grid:attach(M.identity.com.lbl, 8, 1, 1, 1, 'end'  , 'middle')
M.identity.grid:attach(M.identity.com.val, 8, 2, 1, 1, 'start', 'middle')

M.identity.grid:attach(M.identity.fov.lbl, 9, 1, 1, 1, 'end'  , 'middle')
M.identity.grid:attach(M.identity.fov.val, 9, 2, 1, 1, 'start', 'middle')

M.identity.grid:attach(M.identity.uisz.lbl, 10, 1, 1, 1, 'end'  , 'middle')
M.identity.grid:attach(M.identity.uisz.val, 10, 2, 1, 1, 'start', 'middle')

M.outerbox:pushback(ui.separator('horizontal'), 'fill', false)
M.outerbox:pushback(lbltxt('Context'), 'middle', false)
M.context = {
    grid    = ui.grid(15, 2),
    srvaddr = { lbl = lbltxt('Server Address:')  , val = valtxt('(srvaddr)') },
    mapid   = { lbl = lbltxt('Map ID:')          , val = valtxt('(mapid)') },
    maptype = { lbl = lbltxt('Map Type:')        , val = valtxt('(maptype)') },
    shardid = { lbl = lbltxt('Shard ID:')        , val = valtxt('(shardid)') },
    inst    = { lbl = lbltxt('Instance:')        , val = valtxt('(instance)') },
    build   = { lbl = lbltxt('Build ID:')        , val = valtxt('(buildid)') },
    uistate = { lbl = lbltxt('UI State:')        , val = valtxt('(uistate)') },
    compw   = { lbl = lbltxt('Compass Width:')   , val = valtxt('(compwidth)') },
    comph   = { lbl = lbltxt('Compass Height:')  , val = valtxt('(compheight)') },
    compr   = { lbl = lbltxt('Compass Rotation:'), val = valtxt('(comprotation)') },
    ppos    = { lbl = lbltxt('Player Position:') , val = valtxt('(x, y)') },
    mapcnt  = { lbl = lbltxt('Map Center:')      , val = valtxt('(x, y)') },
    mapscl  = { lbl = lbltxt('Map Scale:')       , val = valtxt('(mapscale)') },
    procid  = { lbl = lbltxt('Process ID:')      , val = valtxt('(procid)') },
    mount   = { lbl = lbltxt('Mount:')           , val = valtxt('(mount)') },
}

M.outerbox:pushback(M.context.grid, 'fill', false)
M.context.grid:colspacing(4)
M.context.grid:rowspacing(2)

M.context.grid:attach(M.context.srvaddr.lbl, 1, 1, 1, 1, 'end'  , 'middle')
M.context.grid:attach(M.context.srvaddr.val, 1, 2, 1, 1, 'start', 'middle')

M.context.grid:attach(M.context.mapid.lbl, 2, 1, 1, 1, 'end'  , 'middle')
M.context.grid:attach(M.context.mapid.val, 2, 2, 1, 1, 'start', 'middle')

M.context.grid:attach(M.context.maptype.lbl, 3, 1, 1, 1, 'end'  , 'middle')
M.context.grid:attach(M.context.maptype.val, 3, 2, 1, 1, 'start', 'middle')

M.context.grid:attach(M.context.shardid.lbl, 4, 1, 1, 1, 'end'  , 'middle')
M.context.grid:attach(M.context.shardid.val, 4, 2, 1, 1, 'start', 'middle')

M.context.grid:attach(M.context.inst.lbl, 5, 1, 1, 1, 'end'  , 'middle')
M.context.grid:attach(M.context.inst.val, 5, 2, 1, 1, 'start', 'middle')

M.context.grid:attach(M.context.build.lbl, 6, 1, 1, 1, 'end'  , 'middle')
M.context.grid:attach(M.context.build.val, 6, 2, 1, 1, 'start', 'middle')

M.context.grid:attach(M.context.uistate.lbl, 7, 1, 1, 1, 'end'  , 'middle')
M.context.grid:attach(M.context.uistate.val, 7, 2, 1, 1, 'start', 'middle')

M.context.grid:attach(M.context.compw.lbl, 8, 1, 1, 1, 'end'  , 'middle')
M.context.grid:attach(M.context.compw.val, 8, 2, 1, 1, 'start', 'middle')

M.context.grid:attach(M.context.comph.lbl, 9, 1, 1, 1, 'end'  , 'middle')
M.context.grid:attach(M.context.comph.val, 9, 2, 1, 1, 'start', 'middle')

M.context.grid:attach(M.context.compr.lbl, 10, 1, 1, 1, 'end'  , 'middle')
M.context.grid:attach(M.context.compr.val, 10, 2, 1, 1, 'start', 'middle')

M.context.grid:attach(M.context.ppos.lbl, 11, 1, 1, 1, 'end'  , 'middle')
M.context.grid:attach(M.context.ppos.val, 11, 2, 1, 1, 'start', 'middle')

M.context.grid:attach(M.context.mapcnt.lbl, 12, 1, 1, 1, 'end'  , 'middle')
M.context.grid:attach(M.context.mapcnt.val, 12, 2, 1, 1, 'start', 'middle')

M.context.grid:attach(M.context.mapscl.lbl, 13, 1, 1, 1, 'end'  , 'middle')
M.context.grid:attach(M.context.mapscl.val, 13, 2, 1, 1, 'start', 'middle')

M.context.grid:attach(M.context.procid.lbl, 14, 1, 1, 1, 'end'  , 'middle')
M.context.grid:attach(M.context.procid.val, 14, 2, 1, 1, 'start', 'middle')

M.context.grid:attach(M.context.mount.lbl, 15, 1, 1, 1, 'end'  , 'middle')
M.context.grid:attach(M.context.mount.val, 15, 2, 1, 1, 'start', 'middle')

M.win:show()

local function formatxyz(x, y, z)
    return string.format('% 10.4f, % 10.4f, % 10.4f', x, y, z)
end

local function formatxy(x, y)
    return string.format('% 9.2f, % 9.2f', x, y)
end

function M.onupdate()
    M.ml_name.val:text(string.format('%s', ml.name()))
    M.ml_version.val:text(string.format('%s', ml.version()))
    M.ml_tick.val:text(ml.tick())

    M.ml_apos.val:text(formatxyz(ml.avatarposition()))
    M.ml_afront.val:text(formatxyz(ml.avatarfront()))
    M.ml_atop.val:text(formatxyz(ml.avatartop()))

    M.ml_cpos.val:text(formatxyz(ml.cameraposition()))
    M.ml_cfront.val:text(formatxyz(ml.camerafront()))
    M.ml_ctop.val:text(formatxyz(ml.cameratop()))

    M.identity.name.val:text   (ml.identity.name()           or '(none)')
    M.identity.race.val:text   (ml.identity.racename()       or '(none)')
    M.identity.prof.val:text   (ml.identity.professionname() or '(none)')
    M.identity.spec.val:text   (ml.identity.spec()           or '(none)')
    M.identity.mapid.val:text  (ml.identity.mapid()          or '(none)')
    M.identity.worldid.val:text(ml.identity.worldid()        or '(none)')
    M.identity.teamcol.val:text(ml.identity.teamcolorid()    or '(none)')

    M.identity.com.val:text    (string.format('%s', ml.identity.commander() or false))
    M.identity.fov.val:text    (string.format('%.4f', ml.identity.fov() or 0.0))
    M.identity.uisz.val:text   (ml.identity.uiszname()       or '(none)')

    M.context.srvaddr.val:text(ml.context.serveraddress())
    M.context.mapid.val:text(string.format('%s', ml.context.mapid()))
    M.context.maptype.val:text(string.format('%s', ml.context.maptypename()))
    M.context.shardid.val:text(string.format('%s', ml.context.shardid()))
    M.context.inst.val:text(string.format('%s', ml.context.instance()))
    M.context.build.val:text(string.format('%s', ml.context.buildid()))
    M.context.uistate.val:text(string.format('%d', ml.context.uistate()))
    M.context.compw.val:text(string.format('%d', ml.context.compasswidth()))
    M.context.comph.val:text(string.format('%d', ml.context.compassheight()))
    M.context.compr.val:text(string.format('%.4f', ml.context.compassrotation()))
    M.context.ppos.val:text(formatxy(ml.context.playerposition()))
    M.context.mapcnt.val:text(formatxy(ml.context.mapcenter()))
    M.context.mapscl.val:text(string.format('%0.4f', ml.context.mapscale()))
    M.context.procid.val:text(string.format('%d', ml.context.processid()))
    M.context.mount.val:text(ml.context.mountname())
end

overlay.addeventhandler('update', M.onupdate)

return M
