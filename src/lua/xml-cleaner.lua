local logger = require 'logger'

local M = {}

local log = logger.logger:new("xml-cleaner")

function M.cleanxml(xmldata, xmlname)
    local outlines = {}

    local linechars = {}
    for c = 1, xmldata:len() do
        local ch = xmldata:sub(c,c)
        table.insert(linechars, ch)

        if ch=='\n' then
            table.insert(outlines, table.concat(linechars,''))
            linechars = {}
        elseif ch=='&' then
            for lookahead = c + 1, xmldata:len() do
                local lach = xmldata:sub(lookahead,lookahead)

                if lach==';' then break end

                if lach and not lach:match('[%a%d:%.%-]') then
                    log:warn('%s:%d: invalid entity: %s', xmlname, #outlines + 1, xmldata:sub(c, lookahead))

                    -- assume this is a raw ampersand and escape it
                    table.insert(linechars, 'amp;')
                    break
                end
            end
        end
    end

    if #linechars > 0 then
        table.insert(outlines, table.concat(linechars, ''))
    end

    return table.concat(outlines, '')
end

return M
