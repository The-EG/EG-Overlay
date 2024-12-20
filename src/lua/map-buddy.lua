--[[ RST
Map Buddy
=========

.. image:: /images/modules/map-buddy.png

.. warning::
    This module is a work in progress. Things will change!

Map Buddy displays the player's current position in *continent* coordinates and
the name of the nearest waypoint.
]]--

local ui          = require 'eg-overlay-ui'
local uih         = require 'ui-helpers'
local overlay     = require 'eg-overlay'
local gw2         = require 'gw2'
local static      = require 'gw2.static'
local settings    = require 'settings'
local mumble_link = require 'mumble-link'
local logger      = require 'logger'

local mod = {}

local mb_settings = settings.new('map-buddy.lua')
mb_settings:setdefault('window.x', 10)
mb_settings:setdefault('window.y', 10)
mb_settings:setdefault('window.width', 150)
mb_settings:setdefault('window.height', 100)
mb_settings:setdefault('window.show', false)

mod.win = ui.window("Map Buddy", 600, 10, 350, 400)
mod.win:settings(mb_settings, 'window')

local player_pos_c = uih.text("Character Position:", 0xfcba03ff)
local player_pos_cx = uih.text("X ", 0xfcba03ff)
local player_pos_cy = uih.text("Y ", 0xfcba03ff)

local closest_wp = uih.text("Closest WayPoint: ", 0xfcba03ff)

local box = ui.box('vertical')

box:padding(5, 5, 5, 5)

box:pack_end(player_pos_c)
box:pack_end(player_pos_cx)
box:pack_end(player_pos_cy)
box:pack_end(closest_wp)

mod.log = logger.logger:new("map-buddy")

mod.win:set_child(box)

if mb_settings:get('window.show') then mod.win:show() end

local last_update = 0

local waypoints = {}

local function update()
    if not mb_settings:get('window.show') then return end

    local pcx, pcy = gw2.player_continent_coords()

    -- don't update if mumble-link data isn't valid yet
    -- fixes #2
    if not pcx then return end

    player_pos_cx:update_text(string.format('  X %.2f', pcx))
    player_pos_cy:update_text(string.format('  Y %.2f', pcy))

    local now = overlay.time()

    local dur = now - last_update

    if dur >= 1 then
        last_update = now

        local closest = nil
        local least_dist_sq = nil
        for i, wp in ipairs(waypoints) do
            local dist_sq = (pcx - wp.x)^2 + (pcy - wp.y)^2
            if least_dist_sq == nil or least_dist_sq > dist_sq then
                closest = wp
                least_dist_sq = dist_sq
            end
        end

        if closest then
            closest_wp:update_text("Closest WayPoint: " .. closest.name)
        else
            closest_wp:update_text("No waypoints near.")
        end
    end
end

local function on_map_changed()
    waypoints = static.waypointsinmap(mumble_link.mapid)
end

local function primary_action(event)
    if event=='click-left' then
        if mb_settings:get("window.show") then
            mod.win:hide()
            mb_settings:set("window.show", false)
        else
            mod.win:show()
            mb_settings:set("window.show", true)
        end
    end
end

local function on_startup()
    overlay.queueevent('register-module-actions', {
        name = "Map Buddy",
        primary_action = primary_action
    })

    if mumble_link.mapid~=0 then on_map_changed() end
end

overlay.addeventhandler('update', update)
overlay.addeventhandler('startup', on_startup)
overlay.addeventhandler('mumble-link-map-changed', on_map_changed)

return mod
