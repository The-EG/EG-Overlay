local static = require 'gw2.static'
local logger = require 'logger'

local log = logger.logger:new('updatestaticdb')

log:info("Updating GW2 static data...")

static.updateachievements()
static.updatespecializations()
static.updatecontinents()

log:info("Update complete.")