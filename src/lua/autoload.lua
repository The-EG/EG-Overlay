--[[ RST
:orphan:
:nosearch:
]]--

-- Lua garbage collection settings
-- This is a balance between memory usage and performance.
--
-- The default. This is good performance, but it will take a while for memory to
-- be freed. Memory usage will grow over time until it's finally freed, then
-- grow again, etc.
-- collectgarbage('incremental', 200, 100, 13)
--
-- Aggressive collection. This will keep memory usage minimal but it **wil**
-- negatively affect performance.
-- collectgarbage('incremental', 100, 400, 3)
--
-- A nice compromise. This is somewhere in the middle, and should result in
-- fairly minimal unused memory hanging around with a fairly minimal impact on
-- performance.
collectgarbage('incremental', 110, 200, 8)

-- Any modules should be loaded with `require` here
require 'overlay-stats'
require 'console'
require 'main-menu'

require 'mumble-link-events'
require 'mumble-link-info'

require 'map-buddy'
require 'markers'

require 'psna-tracker'

require 'gw2.static'