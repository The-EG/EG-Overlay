-- EG-Overlay
-- Copyright (c) 2025 Taylor Talkington
-- SPDX-License-Identifier: MIT

--[[ RST
utils
=====

.. lua:module:: utils

.. code-block:: lua

    local utils = require 'utils'

Miscellaneous utilities

Functions
---------
]]--

local M = {}

--[[ RST
.. lua:function:: formatkbytes(kbytes)

    :param number kbytes: The raw number of kilobytes to format.
    :return: A formatted string representing kbytes, i.e. ``'3.12 MiB'``.
    :rtype: string

    .. versionhistory::
        :0.3.0: Added
]]--
function M.formatkbytesize(kbytes)
    if kbytes / 1024.0 / 1024.0 > 1 then
        return string.format('%0.2f GiB', kbytes / 1024.0 / 1024.0)
    elseif kbytes / 1024.0 > 1 then
        return string.format('%0.2f MiB', kbytes / 1024.0)
    else
        return string.format('%0.2f KiB', kbytes)
    end
end

--[[ RST
.. lua:function:: durationtostring(miliseconds)

    :param number miliseconds:
    :rtype: string

    .. versionhistory::
        :0.3.0: Added
]]--
function M.durationtostring(milliseconds)
    local milliseconds_bal = milliseconds % 1000.0
    local hours = math.tointeger(math.floor(milliseconds / 1000.0 / 60.0 / 60.0))
    local minutes = math.tointeger(math.floor(milliseconds / 1000.0 / 60.0)) - (hours * 60)
    local seconds = math.tointeger(math.floor(milliseconds / 1000.0)) - (hours * 60 * 60) - (minutes * 60)

    local str = string.format('%d.%03.0f s', seconds, milliseconds_bal)
    if minutes > 0 then str = string.format('%d m %s', minutes, str) end
    if hours > 0 then str = string.format('%d h %s', hours, str) end

    return str
end

return M
