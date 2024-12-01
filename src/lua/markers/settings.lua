local s = require 'settings'

local settings = require('settings').new('markers.lua')

settings:setdefault('drawMarkers', true)
settings:setdefault('showTooltips', true)

return settings
