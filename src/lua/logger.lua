--[[ RST
logger
======

.. lua:module:: logger

.. code-block:: lua

    local logger = require 'logger'

Logging utilities. Modules should use this for any output instead of ``print``
or directly writing to the console, etc.
]]--

local overlay = require 'eg-overlay'

local logger = {}

--[[ RST
Classes
-------

.. lua:class:: logger

    .. code-block:: lua
        :caption: Example

        local log = logger.logger:new("my-module")
        log:debug("A debug message.")
        log:warn("A warning with a %s", variable)
]]--

logger.logger = {}

--[[ RST
    .. lua:method:: new(name)

        Create a new :lua:class:`logger`. The provided name should be
        informative and match the name of the module in most cases.

        The underlying logging options, such as log file name and log level are
        controlled by the overlay. 

        :param name: The name for the logger. This will be displayed next to any
            messages this logger creates.
        :type name: string
        :rtype: logger

        .. versionhistory::
            :0.0.1: Added
]]--
function logger.logger:new(name)
    local log = { name = name }
    setmetatable(log, self)
    self.__index = self
    return log
end


--[[ RST
    .. lua:method:: error(...)

        Log an ``ERROR`` message. The function parameters work exactly the same
        as ``string.format``.

        .. versionhistory::
            :0.0.1: Added
]]--
function logger.logger:error(...)
    overlay.log(self.name, 1, string.format(...))
end

--[[ RST
    .. lua:method:: warn(...)

        Log a ``WARNING`` message. The function parameters work exactly the same
        as ``string.format``.

        .. versionhistory::
            :0.0.1: Added
]]--
function logger.logger:warn(...)
    overlay.log(self.name, 2, string.format(...))
end

--[[ RST
    .. lua:method:: info(...)

        Log a ``INFO`` message. The function parameters work exactly the same as
        ``string.format``.

        .. versionhistory::
            :0.0.1: Added
]]--
function logger.logger:info(...)
    overlay.log(self.name, 3, string.format(...))
end

--[[ RST
    .. lua:method:: debug(...)

        Log a ``DEBUG`` message. The function parameters work exactly the same as
        ``string.format``.

        .. versionhistory::
            :0.0.1: Added
]]--
function logger.logger:debug(...)
    overlay.log(self.name, 4, string.format(...))
end

--[[ RST
    .. versionhistory::
        :0.0.1: Added
]]--

return logger
