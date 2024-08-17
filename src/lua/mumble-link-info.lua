--[[ RST
Mumble-Link Info
================

.. image:: /images/modules/mumble-link-info.png

.. overlay:module:: mumble-link-info

Mumble-Link Info displays the information available from the GW2 MumbleLink
shared memory. It is hidden on :overlay:event:`mumble-link-unavailable`.
]]--
require 'mumble-link-events'

local ui = require 'eg-overlay-ui'
local uih = require 'ui-helpers'
local overlay = require 'eg-overlay'
local mumble_link = require 'mumble-link'

local logger = require 'logger'
local settings = require 'settings'
local gw2static = require 'gw2.static'
local gw2 = require 'gw2'

local mli = {}

mli.logger = logger.logger:new('mumble-link-info')

mli.ml_available = false

local mli_settings = settings.new('mumble-link-info.lua')
mli_settings:set_default('window.x', 10)
mli_settings:set_default('window.y', 10)
mli_settings:set_default('window.width', 150)
mli_settings:set_default('window.height', 100)

mli.win = ui.window("Mumble-Link Info", 10, 10, 350, 400)
mli.win:settings(mli_settings, 'window')

local function position_info(label, box)
    local lbl = uih.text(label..": ", 0xfcba03ff)
    local posx = uih.text("X ")
    local posy = uih.text("Y ")
    local posz = uih.text("Z ")

    box:pack_end(lbl)
    box:pack_end(posx)
    box:pack_end(posy)
    box:pack_end(posz)

    return lbl, posx, posy, posz
end

local character_name = uih.text("Character: ?", 0xfcba03ff)
local character_profession = uih.text("Profession: ?", 0xfcba03ff)
local map_idlbl = uih.text("Map: ?", 0xfcba03ff)

local map_id = 0


local ui_state = uih.text("UI State", 0xfcba03ff)

local box = ui.box('vertical')

box:padding(5, 5, 5, 5)

box:pack_end(character_name)
box:pack_end(character_profession)
box:pack_end(map_idlbl)

local avatar_pos, avatar_posx, avatar_posy, avatar_posz = position_info("Avatar Position", box)
--local avatar_top, avatar_topx, avatar_topy, avatar_topz = position_info("Avatar Top", box)
local avatar_front, avatar_frontx, avatar_fronty, avatar_frontz = position_info("Avatar Front", box)

local cam_pos, cam_posx, cam_posy, cam_posz = position_info("Camera Position", box)
--local cam_top, cam_topx, cam_topy, cam_topz = position_info("Camera Top", box)
local cam_front, cam_frontx, cam_fronty, cam_frontz = position_info("Camera Front", box)

box:pack_end(ui_state);

mli.win:set_child(box)

--mli.win:show()

local function update()
    if not ml_available then return end

    local apos   = mumble_link.avatar_position
    --local atop   = mumble_link.avatar_top
    local afront = mumble_link.avatar_front
    local campos = mumble_link.camera_position
    --local camtop = mumble_link.camera_top
    local camfront = mumble_link.camera_front

    avatar_posx:update_text(string.format('  X % 4.4f', apos.x))
    avatar_posy:update_text(string.format('  Y % 4.4f', apos.y))
    avatar_posz:update_text(string.format('  Z % 4.4f', apos.z))

    -- avatar_topx:update_text(string.format(' X % 4.4f', atop.x))
    -- avatar_topy:update_text(string.format(' Y % 4.4f', atop.y))
    -- avatar_topz:update_text(string.format(' Z % 4.4f', atop.z))

    avatar_frontx:update_text(string.format(' X % 4.4f', afront.x))
    avatar_fronty:update_text(string.format(' Y % 4.4f', afront.y))
    avatar_frontz:update_text(string.format(' Z % 4.4f', afront.z))

    cam_posx:update_text(string.format('  X % 4.4f', campos.x))
    cam_posy:update_text(string.format('  Y % 4.4f', campos.y))
    cam_posz:update_text(string.format('  Z % 4.4f', campos.z))

    -- cam_topx:update_text(string.format('  X % 4.4f', camtop.x))
    -- cam_topy:update_text(string.format('  Y % 4.4f', camtop.y))
    -- cam_topz:update_text(string.format('  Z % 4.4f', camtop.z))

    cam_frontx:update_text(string.format('  X % 4.4f', camfront.x))
    cam_fronty:update_text(string.format('  Y % 4.4f', camfront.y))
    cam_frontz:update_text(string.format('  Z % 4.4f', camfront.z))

    ui_state:update_text(string.format('UI State: %d', mumble_link.ui_state), true)
end

local function on_map_changed()
    map_id = mumble_link.map_id
    local map = gw2static.map(map_id)
    map_idlbl:update_text('Map: ' .. map.name .. ' (' .. tostring(mumble_link.map_id) .. ') ['..mumble_link.map_type..']', true)
end

local function on_available()
    character_name:update_text('Character: ' .. mumble_link.character_name, true)
    character_profession:update_text('Profession: ' .. mumble_link.character_profession, true)
    mli.win:show()
    ml_available = true
end

local function on_unavailable()
    mli.win:hide()
    ml_available = false
end

local function startup()
    if mumble_link.map_id~=0 then
        on_map_changed()
    end
end

overlay.add_event_handler('startup', startup)

overlay.add_event_handler('update', update)
overlay.add_event_handler('mumble-link-available',   on_available)
overlay.add_event_handler('mumble-link-unavailable', on_unavailable)
overlay.add_event_handler('mumble-link-map-changed', on_map_changed)

return mli
