local settings = require 'settings'
local ui = require 'eg-overlay-ui'
local uih = require 'ui-helpers'
local logger = require 'logger'
local overlay = require 'eg-overlay'

local main_menu = {}

main_menu.log = logger.logger:new('main-menu')

local mm_settings = settings.new('main_menu.lua')
mm_settings:set_default('window.x', 10)
mm_settings:set_default('window.y', 10)
mm_settings:set_default('window.width', 150)
mm_settings:set_default('window.height', 100)

main_menu.win = ui.window("EG-Overlay", 10, 10)
main_menu.win:min_size(150,30)
local box = ui.box('vertical')
main_menu.win:set_child(box)
main_menu.win:settings(mm_settings, "window")

main_menu.win:show()

function main_menu.add_item(event, module_actions)
    main_menu.log:debug("Adding item %s", module_actions.name)
    local btn = uih.text_button(module_actions.name)
    btn:event_handler(module_actions.primary_action)
    box:pack_end(btn, false, 'fill')    
end

overlay.add_event_handler('register-module-actions', main_menu.add_item)

return main_menu