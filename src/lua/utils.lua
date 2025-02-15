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

--[[ RST
.. lua:function:: formatnumbercomma(number)

    Format a number to a human friendly string, ie. 1000 becomes "1,000".

    :rtype: string

    .. versionhistory::
        :0.1.0: Added
]]--
-- https://www.gammon.com.au/forum/?id=7805
function M.formatnumbercomma(num)
    assert (type (num) == "number" or
            type (num) == "string")
  
    local result = ""

    -- split number into 3 parts, eg. -1234.545e22
    -- sign = + or -
    -- before = 1234
    -- after = .545e22

    local sign, before, after = string.match (tostring (num), "^([%+%-]?)(%d*)(%.?.*)$")

    -- pull out batches of 3 digits from the end, put a comma before them

    while string.len (before) > 3 do
        result = "," .. string.sub (before, -3, -1) .. result
        before = string.sub (before, 1, -4)  -- remove last 3 digits
    end -- while

    -- we want the original sign, any left-over digits, the comma part,
    -- and the stuff after the decimal point, if any
    return sign .. before .. result .. after
end

return M
