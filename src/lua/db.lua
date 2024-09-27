--[[ RST
db
==

.. lua:module:: db

.. code-block:: lua

    local db = require 'db'

A collection of database utilities.

Functions
---------
]]--

local dbmod = {}


--[[ RST
.. lua:function:: printquery(db, sql[, cols])

    Print the results of a SQL query to the console.

    .. important::
        Since the :lua:class:`sqlite` class returns results as tables the order
        of columns will be arbitrary. The ``cols`` parameter can be specified
        to preserve column order.

    :param db: A :lua:class:`sqlite` database to query.
    :type db: sqlite
    :param sql: A SQL query to execute.
    :type sql: string
    :param cols: A list of columns to display from the results.
    :type cols: sequence

    .. code-block:: lua
        :caption: Example

        -- run from the Lua Console
        static = require 'gw2.static'
        db = require 'db'

        -- print the number of waypoints in Lion's Arch
        db.printquery(static.db, "SELECT COUNT(*) FROM pois WHERE pois.map = 50 AND pois.type = 'waypoint'")
        -- +----------+
        -- | COUNT(*) |
        -- +----------+
        -- |       13 |
        -- +----------+
]]--
function dbmod.printquery(db, sql, cols)
    local s = db:prepare(sql)
    local function rows()
        return s:step()
    end

    local data = {}
    for row in rows do
        table.insert(data, row)
    end

    s:finalize()

    if #data==0 then return end

    if not cols then
        cols = {}
        for k, v in pairs(data[1]) do
            table.insert(cols, k)
        end
    end

    local colwidth = {}
    for i,c in ipairs(cols) do colwidth[c] = #c end

    for i,row in ipairs(data) do
        for i,c in ipairs(cols) do
            local datastr = tostring(row[c])
            local clen = #datastr
            if colwidth[c] < clen then colwidth[c] = clen end
        end
    end

    
    local colseps = {}
    local hdrseps = {}
    for i,c in ipairs(cols) do
        local w = colwidth[c]
        table.insert(colseps,string.rep('-', w+2))
    end
    local rowsep = '+' .. table.concat(colseps, '+') .. '+'

    print(rowsep)
    local s = '| '
    
    for i, c in ipairs(cols) do
        local w = colwidth[c]
        s = s .. c
        s = s .. string.rep(' ', w - #c) .. ' | '
    end

    print(s)
    print(rowsep)

    for i, row in ipairs(data) do
        s = '| '
        for i,c in ipairs(cols) do
            local w = colwidth[c]
            local vstr = tostring(row[c])
            if type(row[c])=='number' then
                s = s .. string.rep(' ', w - #vstr) .. vstr .. ' | '
            else
                s = s .. vstr .. string.rep(' ', w - #vstr) .. ' | '
            end
        end
        print(s)
    end
    print(rowsep)
end

return dbmod

