--[[ RST
Lua Console/Log
===============

.. overlay:module:: console

The Lua Console can be used to run Lua commands interactively and also displays
log messages. Lua commands are run as coroutines, long running commands that
yield properly will not cause the UI to freeze.
]]--

local overlay = require 'eg-overlay'
local ui = require 'eg-overlay-ui'
local uih = require 'ui-helpers'

local settings = require 'settings'

local logger = require 'logger'

local console = {}

local app_settings = overlay.settings()


--[[ RST
Settings
--------

The settings for the Lua Console are stored in ``settings/console.lua.json``.
]]--
local console_settings = settings.new("console.lua")

--[[ RST

.. overlay:modsetting:: window.x
    :type: integer
    :default: 200

    The window position x coordinate.

    .. versionhistory::
        :0.0.1: Added
]]--
console_settings:set_default("window.x", 200)

--[[ RST
.. overlay:modsetting:: window.y
    :type: integer
    :default: 30

    The window position y coordinate.

    .. versionhistory::
        :0.0.1: Added
]]--
console_settings:set_default("window.y", 30)

--[[ RST
.. overlay:modsetting:: window.width
    :type: integer
    :default: 600

    The window width.

    .. versionhistory::
        :0.0.1: Added
]]--
console_settings:set_default("window.width", 600)

--[[ RST
.. overlay:modsetting:: window.height
    :type: integer
    :default: 300

    The window height.

    .. versionhistory::
        :0.0.1: Added
]]--
console_settings:set_default("window.height", 300)

--[[ RST
.. overlay:modsetting:: window.show
    :type: boolean
    :default: false

    Show the Lua Console window?

    .. versionhistory::
        :0.0.1: Added
]]--
console_settings:set_default("window.show", false)

--[[ RST
.. overlay:modsetting:: maxLines
    :type: integer
    :default: 1000

    The maximum lines to show in the console window. This includes
    input/commands, output and log messages.

    .. versionhistory::
        :0.0.1: Added
]]--
console_settings:set_default("maxLines", 1000)

--[[ RST
.. overlay:modsetting:: colors.ERROR
    :type: integer
    :default: 0x911717FF

    The color used to display ERROR messages.

    .. versionhistory::
        :0.0.1: Added
]]--
console_settings:set_default("colors.ERROR"  , 0x911717FF)

--[[ RST
.. overlay:modsetting:: colors.WARNING
    :type: integer
    :default: 0xb58326FF

    The color used to display WARNING messages.
    
    .. versionhistory::
        :0.0.1: Added
]]--
console_settings:set_default("colors.WARNING", 0xb58326FF)

--[[ RST
.. overlay:modsetting:: colors.DEBUG
    :type: integer
    :default: 0x676F80FF

    The color used to display DEBUG messages.
    
    .. versionhistory::
        :0.0.1: Added
]]--
console_settings:set_default("colors.DEBUG"  , 0x676F80FF)

--[[ RST
.. overlay:modsetting:: colors.INFO
    :type: integer
    :default: 0xFFFFFFFF

    The color used to display INFO messages.
    
    .. versionhistory::
        :0.0.1: Added
]]--
console_settings:set_default("colors.INFO"   , 0xFFFFFFFF)

console.win = ui.window("Lua Console/Log", 0, 0)
console.win:min_size(600, 300)
console.win:resizable(true)
console.win:settings(console_settings, "window")

local outer_box = ui.box('vertical')
outer_box:spacing(10)
outer_box:padding(5,5,2,2)

local message_scroll = ui.scroll_view()
message_scroll:scroll_amount(40)

local message_box = ui.box('vertical')

local prompt_text = uih.monospace_text('Lua> ')

local text_entry = ui.text_entry(app_settings:get('overlay.ui.font.pathMono'), app_settings:get('overlay.ui.font.size'))

local prompt_box = ui.box('horizontal')
prompt_box:align('fill')
prompt_box:pack_end(prompt_text, false, 'middle')
prompt_box:pack_end(text_entry, true, 'fill')

outer_box:pack_end(message_scroll, true, 'fill')
outer_box:pack_end(prompt_box, false, 'fill')

message_scroll:set_child(message_box)

console.win:set_child(outer_box)

local win_show = console_settings:get("window.show")

if console_settings:get("window.show") then console.win:show() end

local history = {}
local next_history = nil

function console.add_line(text, color)
    local color = color or settings:get('overlay.ui.colors.text')

    local t = uih.monospace_text(text, color)
    message_box:pack_end(t, false, 'start')

    while message_box:item_count() > console_settings:get('maxLines') do
        message_box:pop_start()
    end

    message_scroll:scroll_max_y()
end

local function run_text(text)
    if text == nil or text == '' then return end

    table.insert(history, text)
    if #history > 20 then
        table.remove(history, 1)
    end
    next_history = #history

    local color = 0xFFFFFFFF

    console.add_line('Lua> '..text, color)

    text_entry:text('')

    local func, load_err = load(text, 'Lua Console', 't')
    if not func then
        console.add_line(load_err, console_settings:get('colors.ERROR'))
        return
    end

    local call_status, call_err = pcall(func)

    if not call_status then
        console.add_line(call_err, console_settings:get('colors.ERROR'))
        return
    end
end

local function on_entry_keydown(key)
    if key=='return' then
        run_text(text_entry:text())
    elseif key=='up' and next_history > 0 then
        text_entry:text(history[next_history])
        next_history = next_history - 1
    elseif key=='down' and next_history < #history then
        next_history = next_history + 1
        text_entry:text(history[next_history])
    end
end

text_entry:on_keydown(on_entry_keydown)

local function primary_action(event)
    if event=='click-left' then
        if console_settings:get("window.show") then
            console.win:hide()
            console_settings:set("window.show", false)
        else
            console.win:show()
            console_settings:set("window.show", true)
        end
    end
end

local function on_log_message(event, data)
    local color = console_settings:get('colors.'..data.level) or console_settings:get('colors.ERROR')

    console.add_line(data.message, color)
end


local function on_startup()
    overlay.queue_event('register-module-actions', {
        name = "Lua Console",
        primary_action = primary_action
    })
end

overlay.add_event_handler('log-message', on_log_message)
overlay.add_event_handler('startup', on_startup)

-- redefine print so that it prints on the console. kind of hacky...but eh?
function print(...)
    local strs = {}

    for i,p in ipairs({...}) do
        table.insert(strs, tostring(p))
    end

    console.add_line(table.concat(strs, ' '), 0xFFFFFFFF) 
end

return console