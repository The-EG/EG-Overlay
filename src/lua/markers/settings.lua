-- EG-Overlay
-- Copyright (c) 2025 Taylor Talkington
-- SPDX-License-Identifier: MIT

--[[ RST
Markers Settings
================

.. overlay:module:: markers

The markers module stores settings in ``settings/markers.lua.json``.

.. overlay:modsetting:: showMarkers
    :type: boolean
    :default: true

.. overlay:modsetting:: showTooltips
    :type: boolean
    :default: true
]]--
local overlay = require 'eg-overlay'
local settings = overlay.settings('markers.lua')

settings:setdefault('showMarkers', true)
settings:setdefault('showTooltips', true)

return settings
