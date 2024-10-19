--[[ RST
utils
=====

.. lua:module:: utils

The :lua:mod:`utils` module is a collection of general utilities that may be
useful to multiple other modules.

Functions
---------
]]--
local M = {}

--[[ RST
.. lua:function:: meterstoinches(meters)

    Convert meters, which is what the MumbleLink data returns, to inches, which
    is what GW2 uses internally for map coordinates.

    :param number meters: A distance or coordinate in meters
    :return: ``meters`` * 39.3701
    :rtype: number

    .. versionhistory::
        :0.1.0: Added
]]--
function M.meterstoinches(meters)
    return meters * 39.3701
end

--[[ RST
.. lua:function:: formatinchesstr(inches)

    Format a number of inches to a human friendly string, such as "5 ft 2 in".

    .. versionhistory::
        :0.1.0: Added
]]--
function M.formatinchesstr(inches)
    local feet = inches // 12.0
    local inr = inches % 12.0

    if feet > 0 and inr > 0 then
        return string.format("%.0f ft %.0f in", feet, inr)
    elseif feet > 0 then
        return string.format("%.0f ft", feet)
    else
        return string.format("%.0f in", inches)
    end
end

return M
