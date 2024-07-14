local ui = require 'eg-overlay-ui'
local overlay = require 'eg-overlay'
local uih = require 'ui-helpers'
local settings = require 'settings'
local logger = require 'logger'

local psna = {}

psna.log = logger.logger:new('psna-tracker')

local psna_settings = settings.new('psna-tracker.lua')
psna_settings:set_default('window.x', 10)
psna_settings:set_default('window.y', 10)
psna_settings:set_default('window.width', 150)
psna_settings:set_default('window.height', 100)
psna_settings:set_default('window.show', false)

psna.win = ui.window("Pact Supply Network Agents", 20, 20)
psna.win:settings(psna_settings, 'window')

local outerbox = ui.box('vertical')
outerbox:padding(5,5,5,5)

psna.timetomovetxt = uih.text('Agents move in: (calculating)')
outerbox:pack_end(psna.timetomovetxt, false, 'start')
outerbox:spacing(5)

local function on_button_click(agent)
    overlay.clipboard_text(agent.chatlinks[psna.weekday])
end

local function pack_agent(agent)
    agent.box:align('fill')
    agent.box:spacing(10)
    agent.box:pack_end(agent.lbl, true, 'middle')
    agent.box:pack_end(agent.btn, true, 'fill')
    local btn_box = ui.box('horizontal')
    btn_box:padding(5,5,2,2)
    btn_box:align('middle')
    btn_box:pack_end(agent.btn_txt, false, 'start')
    agent.btn:set_child(btn_box)
    outerbox:pack_end(agent.box, false, 'fill')
end

local function new_agent(name, locations, chatlinks)
    local agent = {
        box = ui.box('horizontal'),
        lbl = uih.text(name .. ':'),
        btn = ui.button(),
        btn_txt = uih.text('Location'),
        locations = locations,
        chatlinks = chatlinks
    }
    pack_agent(agent)
    agent.btn:event_handler(function(event)
        if event == 'click-left' then on_button_click(agent) end
    end)
    return agent
end

psna.agents = {
    mehem = new_agent(
        'Mehem the Traveled',
        {"Azarr's Arbor", "Restoration Refuge", "Camp Resolve Waypoint", "Town of Prosperity", "Blue Oasis" , "Repair Station", "Camp Resolve Waypoint"},
        {'[&BIkHAAA=]'  , '[&BIcHAAA=]'       , '[&BH8HAAA=]'          , '[&BH4HAAA=]'       , '[&BKsHAAA=]', '[&BJQHAAA=]'   , '[&BH8HAAA=]'}
    ),
    thefox = new_agent(
        'The Fox',
        {"Mabon Waypoint", "Lionguard Waystation Waypoint", "Desider Atum Waypoint", "Swapwatch Post", "Seraph Protectors", "Breth Ayahusasca", "Gallant's Folly"},
        {'[&BDoBAAA=]'   , '[&BEwDAAA=]'                  , '[&BEgAAAA=]'          , '[&BMIBAAA=]'   , '[&BE8AAAA=]'      , '[&BMMCAAA=]'     , '[&BLkCAAA=]' }
    ),
    yana = new_agent(
        'Specialist Yana',
        {"Fort Trinity Waypoint", "Rally Waypoint", "Waste Hollows Waypoint", "Caer Shadowfain", "Armada Harbor", "Shelter Docks", "Augur's Torch"},
        {'[&BO4CAAA=]'          , '[&BNIEAAA=]'   , '[&BKgCAAA=]'           , '[&BP0CAAA=]'    , '[&BP0DAAA=]'  , '[&BJsCAAA=]'  , '[&BBEDAAA=]'}
    ),
    derwena = new_agent(
        'Lady Derwena',
        {"Mudflat Camp", "Marshwatch Haven Waypoint", "Garenhoff"  , "Shieldbluff Waypoint", "Altar Brook Trading Post", "Pearl Islet Waypoint", "Vigil Keep Waypoint"},
        {'[&BC0AAAA=]' , '[&BKYBAAA=]'              , '[&BBkAAAA=]', '[&BKYAAAA=]'         , '[&BIMAAAA=]'             , '[&BNUGAAA=]'         , '[&BJIBAAA=]'}
    ),
    despina = new_agent(
        'Despina Katelyn',
        {"Blue Ice Shining Waypoint", "Ridgerock Camp Waypoint", "Travelen's Waypoint", "Mennerheim" , "Rocklair"   , "Dolyak Pass Waypoint", "Balddistead"},
        {'[&BIUCAAA=]'              , '[&BIMCAAA=]'            , '[&BGQCAAA=]'        , '[&BDgDAAA=]', '[&BF0GAAA=]', '[&BHsBAAA=]'         , '[&BEICAAA=]'}
    ),
    verma = new_agent(
        'Verma Giftrender',
        {"Snow Ridge Camp Waypoint", "Haymal Gore", "Temperus Point Waypoint", "Ferrusatos Village", "Village of Scalecatch Waypoint", "Hawkgates Waypoint", "Bovarin Estate"},
        {'[&BCECAAA=]'             , '[&BA8CAAA=]', '[&BIMBAAA=]'            , '[&BPEBAAA=]'       , '[&BOcBAAA=]'                   , '[&BNMAAAA=]'       , '[&BBABAAA=]'}
    )
}

psna.win:set_child(outerbox)

psna.weekday = 0

psna.lastupdate = os.time()

local function agent_update_txt(agent, weekday)
    agent.btn_txt:update_text(agent.locations[weekday])
end

local function update()
    local now = os.time()
    if os.difftime(now, psna.lastupdate) < 1 then return end

    psna.lastupdate = now

    local date = os.date('!*t')
    local weekday = date.wday

    if date.hour < 8 then
        weekday = weekday - 1
        if weekday < 1 then weekday = 7 end
    end

    local hrstomove
    if date.hour < 8 then
        hrstomove = 8 - date.hour - 1
    else
        hrstomove = 32 - date.hour - 1
    end

    local minstomove = 60 - date.min

    local timetomove = string.format('Agents move in: %d hr %d min', hrstomove, minstomove)

    if weekday~=psna.weekday then
        for n,a in pairs(psna.agents) do
            agent_update_txt(a, weekday)
        end
        psna.weekday = weekday
    end
    
    psna.timetomovetxt:update_text(timetomove)
end

local function primary_action(event)
    if event=='click-left' then
        if psna_settings:get("window.show") then
            psna.win:hide()
            psna_settings:set("window.show", false)
            overlay.remove_event_handler('update', psna.update_handler_id)
            psna.update_handler_id = nil
        else
            psna.win:show()
            psna_settings:set("window.show", true)
            psna.update_handler_id = overlay.add_event_handler('update', update)
        end
    end
end

local function on_startup()
    overlay.queue_event('register-module-actions', {
        name = "PSNA Tracker",
        primary_action = primary_action
    })
end

if psna_settings:get('window.show') then
    psna.update_handler_id = overlay.add_event_handler('update', update)
    psna.win:show()
end

overlay.add_event_handler('startup', on_startup)

return psna