-- EG-Overlay
-- Copyright (c) 2025 Taylor Talkington
-- SPDX-License-Identifier: MIT

require 'mumble-link-events'
require 'markers.manager'
local cm = require 'markers.category-manager'

require 'markers.settings-win'

local overlay_menu = require 'overlay-menu'

overlay_menu.additem('Markers', 'explore', function() cm._cm:oncatmanagerbtnclick() end)

return {}
