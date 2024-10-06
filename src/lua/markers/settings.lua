local s = require 'settings'

local settings = require('settings').new('markers.lua')

settings:set_default('drawMarkers', true)
settings:set_default('showTooltips', true)

return settings
