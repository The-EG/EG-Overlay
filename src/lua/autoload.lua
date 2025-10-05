-- EG-Overlay
-- Copyright (c) 2025 Taylor Talkington
-- SPDX-License-Identifier: MIT

-- This file is automatically loaded when EG-Overlay is starting. You can add
-- code here to control settings, or load modules with `require`.

-- Lua garbage collection settings
-- This is a balance between memory usage and performance.
--
-- The default. This is good performance, but it will take a while for memory to
-- be freed. Memory usage will grow over time until it's finally freed, then
-- grow again, etc.
-- collectgarbage('incremental', 200, 100, 13)
--
-- Aggressive collection. This will keep memory usage minimal but it **will**
-- negatively affect performance.
-- collectgarbage('incremental', 100, 400, 3)
--
-- A nice compromise. This is somewhere in the middle, and should result in
-- fairly minimal unused memory hanging around with a fairly minimal impact on
-- performance.
collectgarbage('incremental', 110, 200, 8)

require 'overlay-menu'

require 'console'
require 'overlay-stats'

require 'psna-tracker'

require 'markers'

