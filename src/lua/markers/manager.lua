local ui = require 'eg-overlay-ui'
local uih = require 'ui-helpers'

local md = require 'markers.data'
local dialogs = require 'ui-dialogs'

local mod = {}

mod.packswin = {
    win       = ui.window('Marker Packs', 30, 30),
    box       = ui.box('vertical'),
    gridbox   = ui.box('vertical'),
    importbtn = uih.text_button('Import Pack'),
    closebtn  = uih.text_button('Close'),
}

mod.packswin.win:set_child(mod.packswin.box)
mod.packswin.box:padding(10, 10, 10, 10)
mod.packswin.box:spacing(10)
mod.packswin.box:pack_end(mod.packswin.gridbox, false, 'fill')
mod.packswin.box:pack_end(ui.separator('horizontal'), false, 'fill')
mod.packswin.box:pack_end(mod.packswin.importbtn, false, 'fill')
mod.packswin.box:pack_end(mod.packswin.closebtn, false, 'fill')

mod.packswin.importbtn:event_handler(function(event)
    if event~='click-left' then return end
    local d = dialogs.openfile:new({
        title = 'Select marker pack',
        onopen = function(path)
            print('Open clicked')
        end,
    })
    d:setlocation('D:/Documents')
    d:show()
end)

--mod.packswin.win:min_size(300, 200)
mod.packswin.win:show()

function mod.updatepackslist()
    local stmt = md.db:prepare('SELECT id, type, path FROM markerpack')
    local function rows()
        return stmt:step()
    end

    local packs = {}
    local packcount = 0
    for row in rows do
        table.insert(packs, row)
        packcount = packcount + 1
    end
    stmt:finalize()

    while mod.packswin.gridbox:item_count() > 0 do
        mod.packswin.gridbox:pop_start()
    end
    
    local grid = ui.grid(packcount + 2, 4)
    grid:colspacing(10)
    grid:rowspacing(5)
    mod.packswin.gridbox:pack_end(grid, true, 'fill')

    grid:attach(uih.text('ID'), 1, 1, 1, 1, 'middle', 'middle')
    grid:attach(uih.text('Path'), 1, 2, 1, 1, 'middle', 'middle')
    grid:attach(uih.text('Type'), 1, 3, 1, 1, 'middle', 'middle')
    grid:attach(ui.separator('horizontal'), 2, 1, 1, 4, 'fill', 'middle')

    local gridrow = 3
    for i, p in ipairs(packs) do
        grid:attach(uih.text(string.format("%d", p.id)), gridrow, 1, 1, 1, 'end', 'middle')
        grid:attach(uih.text(p.path), gridrow, 2, 1, 1, 'start', 'middle')
        grid:attach(uih.text(p.type), gridrow, 3, 1, 1, 'middle', 'middle')

        local delbtn = uih.text_button('X')
        grid:attach(delbtn, gridrow, 4, 1, 1, 'start', 'middle')

        gridrow = gridrow + 1
    end
end

mod.updatepackslist()

return mod
