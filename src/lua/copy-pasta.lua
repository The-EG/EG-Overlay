local overlay = require 'eg-overlay'
local ui = require 'eg-overlay-ui'
local uih = require 'ui-helpers'
local logger = require 'logger'
local json = require 'jansson'

local M = {}

local settings = require('settings').new('copy-pasta.lua')

local log = logger.logger:new('copy-pasta')

settings:setdefault('window.x', 200)
settings:setdefault('window.y', 50)
settings:setdefault('window.width', 200)
settings:setdefault('window.height', 400)
settings:setdefault('window.show', false)

local bowl = {}
bowl.__index = bowl

function bowl:new(name)
    local o = {
        name = name,
        pastas = {}
    }

    setmetatable(o, self)

    return o
end

function bowl:add(child)
    table.insert(self.pastas, child)
end

local pasta = {}
pasta.__index = pasta

function pasta:new(name, text)
    local o = {
        name = name,
        text = text,
    }

    setmetatable(o, self)

    return o
end

function pasta:copy()
    overaly.clipboardtext(self.text)
end

local function pastabtn(pasta)
    local b = ui.button()
    local box = ui.box('horizontal')
    local t = uih.text(pasta.name)
    box:padding(5,5,2,2)
    box:spacing(5)
    box:pack_end(t, false, 'start')
    b:set_child(box)
    b:background_color(0)
    b:border_width(0)

    if getmetatable(pasta)==bowl then
        local a = uih.text('->', true)
        box:pack_end(a, false, 'start')
    end

    return b
end


local copypastawin = {
    bowlstack = {}
}

function copypastawin:setup()
    self.win = ui.window('Copy Pasta', 20, 20)
    self.win:min_size(100, 200)
    self.win:resizable(true)
    self.win:settings(settings, 'window')

    self.outerbox = ui.box('vertical')
    self.outerbox:padding(5, 5, 5, 5)
    self.outerbox:spacing(2)
    self.outerbox:align('start')
    self.win:set_child(self.outerbox)

    self.pathbox = ui.box('vertical')
    self.pathbox:padding(0, 0, 2, 2)
    self.pathbox:spacing(2)
    self.outerbox:pack_end(self.pathbox, false, 'fill')

    self.outerbox:pack_end(ui.separator('horizontal'), false, 'fill')

    self.scroll = ui.scrollview()
    self.outerbox:pack_end(self.scroll, true, 'fill')

    self.outerbox:pack_end(ui.separator('horizontal'), false, 'fill')

    self.closebtn = uih.text_button('Close')
    self.closebtn:addeventhandler(function(event)
        if event~='click-left' then return end
        self:hide()
    end)
    self.outerbox:pack_end(self.closebtn, false, 'fill')
end

function copypastawin:show()
    self.win:show()
    settings:set('window.show', true)
end

function copypastawin:hide()
    self.win:hide()
    settings:set('window.show', false)
end

function copypastawin:loadpastas()
    local function loadjson(fromjson, tobowl)
        for k, v in pairs(fromjson) do
            if type(v)=='string' then
                local p = pasta:new(k, v)
                tobowl:add(p)
            else
                local b = bowl:new(k)
                tobowl:add(b)
                loadjson(v, b)
            end
        end
    end

    local datafolder = overlay.datafolder('copy-pasta')

    self.toplevelbowl = bowl:new('root')

    local r, files = pcall(overlay.findfiles, datafolder .. '*.json')

    if not r then
        log:error("Couldn't load pastas from %s: %s", datafolder, files)
        return
    end

    log:info("Loading pastas from %s...", datafolder)

    for i, f in ipairs(files) do
        local nm = f.name:match('(.*)%.json')
        log:debug("Loading %s", nm)
        local r, j = pcall(json.loadfile, datafolder .. f.name)
        
        if not r then
            log:error("Couldn't parse %s: %s", nm, j)
            goto nextfile
        end

        local b = bowl:new(nm)
        self.toplevelbowl:add(b)
        loadjson(j, b)

        ::nextfile::
    end
end

function copypastawin:reload()
    while self.pathbox:item_count()>0 do self.pathbox:pop_end() end

    local homebtn = uih.text_button('Home')
    self.pathbox:pack_end(homebtn, false, 'fill')

    homebtn:addeventhandler(function (e)
        if e~='click-left' then return end
        self.bowlstack = {}
        self:reload()
    end)

    for i, b in ipairs(self.bowlstack) do
        local btn = uih.text_button(b.name)
        self.pathbox:pack_end(btn, false, 'fill')
        btn:addeventhandler(function(e)
            if e~='click-left' then return end
            while #self.bowlstack > i do table.remove(self.bowlstack) end
            self:reload()
        end)
    end

    local b = self.bowlstack[#self.bowlstack] or self.toplevelbowl

    local box = ui.box('vertical')
    box:spacing(5)
    self.scroll:set_child(box)

    for i, p in ipairs(b.pastas) do
        if getmetatable(p)==bowl then
            local btn = pastabtn(p)
            box:pack_end(btn, false, 'fill')
            btn:addeventhandler(function(e)
                if e~='click-left' then return end
                table.insert(self.bowlstack, p)
                self:reload()
            end)
        else
            local btn = pastabtn(p)
            box:pack_end(btn, false, 'fill')
            btn:addeventhandler(function(e)
                if e~='click-left' then return end

                overlay.clipboardtext(p.text)
            end)
        end
    end
end

local function onprimaryaction()
    if settings:get('window.show') then
        copypastawin:hide()
    else
        copypastawin:show()
    end
end

local function onstartup()
    copypastawin:setup()
    copypastawin:loadpastas()
    copypastawin:reload()

    if settings:get('window.show') then
        copypastawin:show()
    end

    overlay.queueevent("register-module-actions", {
        name = "Copy Pasta",
        primary_action = onprimaryaction,
    })
end

overlay.addeventhandler('startup', onstartup)

return M
