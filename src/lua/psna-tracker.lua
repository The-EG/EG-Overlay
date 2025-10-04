-- EG-Overlay
-- Copyright (c) 2025 Taylor Talkington
-- SPDX-License-Identifier: MIT

--[[ RST
Pact Supply Network Agents Tracker
==================================

.. overlay:module:: psna-tracker

The Pact Supply Network Agents Tracker shows the current locations of the PSN
agents as well as a countdown to when they move next.

The nearest waypoint or POI will be copied to the clipboard when the name of a
location is clicked.
]]--

local overlay = require 'overlay'
local ui = require 'ui'

local overlay_menu = require 'overlay-menu'

local M = {}

M.psnawin = {}

function M:init()
    self.settings = overlay.settings('psna-tracker.lua')
    self.settings:setdefault('window.x', 10)
    self.settings:setdefault('window.y', 10)
    self.settings:setdefault('window.width', 150)
    self.settings:setdefault('window.height', 100)
    self.settings:setdefault('window.show', false)

    self.win = ui.window('Pact Supply Network Agents')
    self.win:settings(self.settings, 'window')

    self.outerbox = ui.box('vertical')
    self.outerbox:paddingleft(5)
    self.outerbox:paddingright(5)
    self.outerbox:paddingtop(5)
    self.outerbox:paddingbottom(5)

    self.win:child(self.outerbox)

    self.timetomovetext = ui.text('Agents move in: (calculating)', ui.color('text'), ui.fonts.regular)

    self.grid = ui.grid(8, 2)

    self.grid:rowspacing(2)
    self.grid:colspacing(1, 5)

    self.outerbox:pushback(self.grid, 'start', false)

    self.grid:attach(self.timetomovetext, 1, 1, 1, 2, 'middle', 'start')
    self.grid:attach(ui.separator('horizontal'), 2, 1, 1, 2, 'fill', 'middle')

    self._nextgridrow = 3
    self.weekday = 0
    self.lastupdate = os.time()

    self.agents = {
        mehem = self:newagent(
            'Mehem the Traveled',
            {
                "Azarr's Arbor",
                "Restoration Refuge",
                "Camp Resolve Waypoint",
                "Town of Prosperity",
                "Blue Oasis",
                "Repair Station",
                "Camp Resolve Waypoint"
            },
            {
                '[&BIkHAAA=]',
                '[&BIcHAAA=]',
                '[&BH8HAAA=]',
                '[&BH4HAAA=]',
                '[&BKsHAAA=]',
                '[&BJQHAAA=]',
                '[&BH8HAAA=]'
            }
        ),
        thefox = self:newagent(
            'The Fox',
            {
                "Mabon Waypoint",
                "Lionguard Waystation Waypoint",
                "Desider Atum Waypoint",
                "Swapwatch Post",
                "Seraph Protectors",
                "Breth Ayahusasca",
                "Gallant's Folly"
            },
            {
                '[&BDoBAAA=]',
                '[&BEwDAAA=]',
                '[&BEgAAAA=]',
                '[&BMIBAAA=]',
                '[&BE8AAAA=]',
                '[&BMMCAAA=]',
                '[&BLkCAAA=]'
            }
        ),
        yana = self:newagent(
            'Specialist Yana',
            {
                "Fort Trinity Waypoint",
                "Rally Waypoint",
                "Waste Hollows Waypoint",
                "Caer Shadowfain",
                "Armada Harbor",
                "Shelter Docks",
                "Augur's Torch"
            },
            {
                '[&BO4CAAA=]',
                '[&BNIEAAA=]',
                '[&BKgCAAA=]',
                '[&BP0CAAA=]',
                '[&BP0DAAA=]',
                '[&BJsCAAA=]',
                '[&BBEDAAA=]'
            }
        ),
        derwena = self:newagent(
            'Lady Derwena',
            {
                "Mudflat Camp",
                "Marshwatch Haven Waypoint",
                "Garenhoff",
                "Shieldbluff Waypoint",
                "Altar Brook Trading Post",
                "Pearl Islet Waypoint",
                "Vigil Keep Waypoint"
            },
            {
                '[&BC0AAAA=]',
                '[&BKYBAAA=]',
                '[&BBkAAAA=]',
                '[&BKYAAAA=]',
                '[&BIMAAAA=]',
                '[&BNUGAAA=]',
                '[&BJIBAAA=]'
            }
        ),
        despina = self:newagent(
            'Despina Katelyn',
            {
                "Blue Ice Shining Waypoint",
                "Ridgerock Camp Waypoint",
                "Travelen's Waypoint",
                "Mennerheim",
                "Rocklair",
                "Dolyak Pass Waypoint",
                "Balddistead"
            },
            {
                '[&BIUCAAA=]',
                '[&BIMCAAA=]',
                '[&BGQCAAA=]',
                '[&BDgDAAA=]',
                '[&BF0GAAA=]',
                '[&BHsBAAA=]',
                '[&BEICAAA=]'
            }
        ),
        verma = self:newagent(
            'Verma Giftrender',
            {
                "Snow Ridge Camp Waypoint",
                "Haymal Gore",
                "Temperus Point Waypoint",
                "Ferrusatos Village",
                "Village of Scalecatch Waypoint",
                "Hawkgates Waypoint",
                "Bovarin Estate"
            },
            {
                '[&BCECAAA=]',
                '[&BA8CAAA=]',
                '[&BIMBAAA=]',
                '[&BPEBAAA=]',
                '[&BOcBAAA=]',
                '[&BNMAAAA=]',
                '[&BBABAAA=]'
            }
        ),
    }
end

function M:onmenuclick()
    if self.visible then self:hide() else self:show() end
end

function M:show()
    if not self.visible then
        self.win:show()
        self.settings:set('window.show', true)
        self.visible = true
    end
end

function M:hide()
    if self.visible then
        self.win:hide()
        self.settings:set('window.show', false)
        self.visible = false
    end
end

function M:onagentclick(agent)
    overlay.clipboardtext(agent.chatlinks[self.weekday])
end

function M:newagent(name, locations, chatlinks)
    local agent = {
        loc_txt = ui.text('(Updating...)', ui.color('text'), ui.fonts.regular),
        locations = locations,
        chatlinks = chatlinks,
    }

    local lbl = ui.text(string.format('%s:', name), ui.color('text'), ui.fonts.regular)
    local btn = ui.button()
    local btnbox = ui.box('horizontal')

    btnbox:paddingleft(5)
    btnbox:paddingright(5)
    btnbox:paddingtop(5)
    btnbox:paddingbottom(5)
    btnbox:alignment('middle')

    btnbox:pushback(agent.loc_txt, 'start', false)
    btn:child(btnbox)

    self.grid:attach(lbl, self._nextgridrow, 1, 1, 1, 'end', 'middle')
    self.grid:attach(btn, self._nextgridrow, 2, 1, 1, 'fill', 'fill')
    self._nextgridrow = self._nextgridrow + 1

    btn:addeventhandler(function()
        self:onagentclick(agent)
    end, 'click-left')

    return agent
end

function M:updateagenttext(agent, weekday)
    agent.loc_txt:text(agent.locations[weekday])
end

function M:onupdate()
    if not self.visible then return end

    local now = os.time()
    if os.difftime(now, self.lastupdate) < 1 then return end

    self.lastupdate = now

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

    if weekday~=self.weekday then
        for n,a in pairs(self.agents) do
            self:updateagenttext(a, weekday)
        end
        self.weekday = weekday
    end

    self.timetomovetext:text(timetomove)
end

M:init()

if M.settings:get('window.show') then
    M:show()
end

overlay.addeventhandler('update', function() M:onupdate() end)

overlay_menu.additem('PSNA Tracker', 'travel_explore', function() M:onmenuclick() end)

return M
