-- EG-Overlay
-- Copyright (c) 2025 Taylor Talkington
-- SPDX-License-Identifier: MIT
local overlay = require 'overlay'
local markerspackage = require 'markers.package'
local ui = require 'ui'
local dialogs = require 'dialogs'
local path = require 'path'

local M = {}

local function strtoint(str)
    local num = tonumber(str)

    if str=='true' then
        return 1
    elseif str=='false' then
        return 0
    end

    if not num then
        return nil
    end

    local int = math.tointeger(num)

    if not int then
                return nil
    end

    return int
end

local function strtofloat(str)
    local num = tonumber(str)

    if not num then
        return nil
    end

    return num
end

local function colortoint(color)
    local c = color

    if string.sub(c,1,1)=='#' then c = string.sub(c, 2) end

    if #c~=6 and #c~=8 then
        overlay.logerror(string.format("Invalid color string: %s", color))
        return nil
    end

    local colorint = tonumber(c, 16)

    if not colorint then
        return nil
    end

    if #c == 6 then
        colorint = (colorint << 16) + 0xFF -- add alpha component
    end

    return colorint
end

local function strtotable(str)
    local tbl = {}
    for val in string.gmatch(str, '([^,]+)') do
        table.insert(tbl, val)
    end
    return tbl
end

local function unchanged(str)
    return str
end

local function strtopath(str)
    -- Teh's trails has some file paths with \ instead of /
    return string.gsub(str, '\\','/')
end

local fromxml = {
    ['achievementid'     ] = strtoint,
    ['achievementbit'    ] = strtoint,
    ['alpha'             ] = strtofloat,
    ['animspeed'         ] = strtofloat,
    ['autotrigger'       ] = strtoint,
    ['behavior'          ] = strtoint,
    ['bounce'            ] = unchanged,
    ['bounce-height'     ] = strtofloat,
    ['bounce-delay'      ] = strtofloat,
    ['bounce-duration'   ] = strtofloat,
    ['canfade'           ] = strtoint,
    ['color'             ] = colortoint,
    ['copy'              ] = unchanged,
    ['copy-message'      ] = unchanged,
    ['cull'              ] = unchanged,
    ['defaulttoggle'     ] = strtoint,
    ['toggledefault'     ] = strtoint,
    ['displayname'       ] = unchanged,
    ['fadenear'          ] = strtofloat,
    ['fadefar'           ] = strtofloat,
    ['festival'          ] = unchanged,
    ['guid'              ] = unchanged,
    ['hascountdown'      ] = strtoint,
    ['heightoffset'      ] = strtofloat,
    ['hide'              ] = unchanged,
    ['iconfile'          ] = unchanged,
    ['iconsize'          ] = strtofloat,
    ['info'              ] = unchanged,
    ['inforange'         ] = strtofloat,
    ['ingamevisibility'  ] = strtoint,
    ['invertbehavior'    ] = strotint,
    ['ishidden'          ] = strtoint,
    ['isseparator'       ] = strtoint,
    ['iswall'            ] = strtoint,
    ['mapdisplaysize'    ] = strtofloat,
    ['mapid'             ] = strtoint,
    ['maptype'           ] = unchanged,
    ['mapvisibility'     ] = strtoint,
    ['minimapvisibility' ] = strtoint,
    ['minsize'           ] = strtofloat,
    ['maxsize'           ] = strtofloat,
    ['mount'             ] = unchanged,
    ['name'              ] = unchanged,
    ['occlude'           ] = strtoint,
    ['xpos'              ] = strtofloat,
    ['ypos'              ] = strtofloat,
    ['zpos'              ] = strtofloat,
    ['raid'              ] = unchanged,
    ['profession'        ] = unchanged,
    ['race'              ] = unchanged,
    ['resetguid'         ] = unchanged,
    ['resetlength'       ] = strtofloat,
    ['resetoffset'       ] = strtoint,
    ['rotate'            ] = unchanged,
    ['rotate-x'          ] = strtofloat,
    ['rotate-y'          ] = strtofloat,
    ['rotate-z'          ] = strtofloat,
    ['scaleonmapwithzoom'] = strtoint,
    ['schedule'          ] = unchanged,
    ['script-tick'       ] = unchanged,
    ['script-focus'      ] = unchanged,
    ['script-trigger'    ] = unchanged,
    ['script-filter'     ] = unchanged,
    ['script-once'       ] = unchanged,
    ['show'              ] = unchanged,
    ['specialization'    ] = unchanged,
    ['texture'           ] = unchanged,
    ['tip-name'          ] = unchanged,
    ['tip-description'   ] = unchanged,
    ['toggle'            ] = unchanged,
    ['togglecategory'    ] = unchanged,
    ['traildata'         ] = strtopath,
    ['trailscale'        ] = unchanged,
    ['triggerrange'      ] = strtofloat,
    ['type'              ] = unchanged
}

M.taco2db = {}
M.taco2db.__index = M.taco2db

function M.taco2db.new(tacopath, dbpath)
    local c = {
        tacopath = tacopath,
        dbpath   = dbpath,

        category_stack = {},

        categories = {},
        categories_ins = {},
        pois = {},
        trails = {},
    }

    setmetatable(c, M.taco2db)

    return c
end

function M.taco2db:createdatafile(path)
    local df = self.mp:datafile(path)

    if df:data() == nil then
        local data = self.taco:content(path)

        if not data then
            return false
        end
        df:data(data)
    end

    return true
end

function M.taco2db:createcategory(typeid, props)
    local cat = self.mp:category(typeid, true)

    for p,v in pairs(props) do
        cat[p] = v
    end

    if props.iconfile then
        if not self:createdatafile(props.iconfile) then
            overlay.logwarn(string.format('iconfile %s does not exist.', props.iconfile))
        end
    end

    if props.texture then
        if not self:createdatafile(props.texture) then
            overlay.logwarn(string.format('texture %s does not exist.', props.texture))
        end
    end
end

function M.taco2db:createpoi(cat, props)
    local marker = cat:newmarker()

    if props.mapid==nil then
        overlay.logwarn('POI with no MapID')
    else
        marker:mapid(props.mapid)
    end

    for p, v in pairs(props) do
        marker[p] = v
    end

    if props.iconfile then
        if not self:createdatafile(props.iconfile) then
            overlay.logwarn(string.format('iconfile %s does not exist.', props.iconfile))
        end
    end
end

function M.taco2db:createtrail(cat, props)
    local t = cat:newtrail()

    for p,v in pairs(props) do t[p] = v end

    local trail_file = self.taco:content(props.traildata)

    if not trail_file then
        overlay.logerror(string.format('traildata %s does not exist.', props.traildata))
        return
    end

    local trail_file_len = #trail_file

    local trailver, map_id = string.unpack('<I4I4', trail_file)

    -- todo: check trailver == 0

    if not props.mapid then t.mapid = map_id end

    t:mapid(map_id)

    local trail_file_pos = 9
    repeat
        local x, y, z, l = string.unpack('<fff', trail_file, trail_file_pos)
        trail_file_pos = l

        t:addpoint(x, y, z)
    until trail_file_pos > trail_file_len

    if props.texture then
        if not self:createdatafile(props.texture) then
            overlay.logwarn(string.format('text %s does not exist.', props.texture))
        end
    end
end

-- cleanup bad xml, mostly those that have bare & instead of &amp;
local function cleanxml(xmldata, xmlname)
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
                    overlay.logwarn(string.format('%s:%d: invalid entity: %s', xmlname, #outlines + 1, xmldata:sub(c, lookahead)))

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

function M.taco2db:run()
    local start = overlay.time()

    overlay.loginfo(string.format('Loading %s:', self.tacopath))

    self.taco = overlay.openzip(self.tacopath)
    if not self.taco then
        error(string.format("Couldn't load taco file: %s", self.tacopath))
    end

    -- root directory XML files
    local files = {}
    for i,e in ipairs(self.taco:entries()) do
        if not e.is_directory and string.find(e.name, '[^/]+%.xml') == 1 then
            table.insert(files, e.name)
        end
    end

    table.sort(files)

    for i,f in ipairs(files) do
        overlay.loginfo(string.format('  %s', f))

        local xml = cleanxml(self.taco:content(f), f)

        overlay.parsexml(xml, function(event, data) self:xmlevent(event, data) end)
        coroutine.yield('Parsing XML:',i, #files)
    end

    local typeids = {}
    for k,v in pairs(self.categories) do table.insert(typeids, k) end
    table.sort(typeids)

    overlay.loginfo(string.format("Loaded %d categories, %d POIs, %d trails.", #typeids, #self.pois, #self.trails))
    overlay.loginfo(string.format('Creating %s:', self.dbpath))

    self.mp = markerspackage.markerpack:new(self.dbpath)
    self.mp.db:execute('BEGIN')

    overlay.loginfo('Creating categories...')

    for i,cats in ipairs(self.categories_ins) do
        local typeid = cats.typeid
        local props = cats.props

        self:createcategory(typeid, props)

        coroutine.yield('Creating categories:', i, #self.categories_ins)
    end

    overlay.loginfo('Creating POIs...')

    for i, poi in ipairs(self.pois) do
        if poi.type~=nil then
            local cat = self.mp:category(poi.type)
            if cat then
                self:createpoi(cat, poi)
            else
                overlay.logwarn(string.format('POI in category that does not exist: %s', poi.type))
            end
        else
            overlay.logwarn(string.format('POI with no typeid, ignoring...'))
        end
        coroutine.yield('Creating POIs:', i, #self.pois)
    end

    overlay.loginfo('Creating Trails...')
    for i, trail in ipairs(self.trails) do
        if trail.type~=nil then
            if not trail.traildata then
                overlay.logwarn('Trail with no traildata, ignoring...')
            else
                local cat = self.mp:category(trail.type)
                if cat then
                    self:createtrail(cat, trail)
                else
                    overlay.logwarn(string.format('Trail in category that does not exist: %s', trail.type))
                end
            end
        else
            overlay.logwarn("Trail with no typeid, ignoring...")
        end
        coroutine.yield('Creating trails:', i, #self.trails)
    end

    self.mp.db:execute('COMMIT')

    overlay.loginfo('Done, optimizing database...')
    self.mp.db:execute('PRAGMA optimize')
    coroutine.yield('Optimizing Database...')
    self.mp.db:execute('VACUUM')

    local finish = overlay.time()

    coroutine.yield('Done.')

    local duration = finish - start
    local durstr = require('utils').durationtostring(duration * 1000.0)

    overlay.loginfo(string.format('Conversion complete in %s.', durstr))

    self.categories = {}
    self.pois = {}
    self.trails = {}

    self.mp:close()
    self.mp = nil
end

function M.taco2db:xmlevent(event, data)
    if     event == 'start-element' then self:xmlstartelement(data)
    elseif event == 'end-element'   then self:xmlendelement(data)
    end
end

function M.taco2db:xmlstartelement(data)
    if data.name.local_name == 'OverlayData' then
        self.in_overlay_data = true
    elseif data.name.local_name == 'MarkerCategory' then
        self:processcategory(data)
    elseif data.name.local_name == 'POI' then
        self:processpoi(data)
    elseif data.name.local_name == 'Trail' then
        self:processtrail(data)
    end
end

function M.taco2db:xmlendelement(data)
    if data.local_name == 'OverlayData' then
        self.in_overlay_data = false
    elseif data.local_name == 'MarkerCategory' then
        table.remove(self.category_stack)
    end
end

function M.taco2db:processcategory(data)
    local attrs = {}
    for i,att in ipairs(data.attributes) do
        local attrname = string.lower(att.name.local_name)
        local val = att.value

        if fromxml[attrname] then val = fromxml[attrname](val) end

        attrs[attrname] = val
    end

    if not attrs.name and attrs['bh-name'] then attrs.name = attrs['bh-name'] end

    if not attrs.name then
        overlay.logerror("Category without a name, ignoring!")
        return
    end

    table.insert(self.category_stack, attrs.name)

    local fullname = table.concat(self.category_stack, '.')
    if not self.categories[fullname] then
        self.categories[fullname] = true
        table.insert(self.categories_ins, {typeid=fullname, props=attrs})
    end
end

function M.taco2db:processpoi(data)
    local poi = {}
    for i,att in ipairs(data.attributes) do
        local attrname = string.lower(att.name.local_name)
        local val = att.value

        if fromxml[attrname] then val = fromxml[attrname](val) end

        poi[attrname] = val
    end

    table.insert(self.pois, poi)
end

function M.taco2db:processtrail(data)
    local trail = {}
    for i,att in ipairs(data.attributes) do
        local attrname = string.lower(att.name.local_name)
        local val = att.value

        if fromxml[attrname] then val = fromxml[attrname](val) end

        trail[attrname] = val
    end

    table.insert(self.trails, trail)
end

M.wizard = {}
M.wizard.__index = M.wizard

local function txtbutton(text)
    local btn = ui.button()
    local box = ui.box('vertical')
    local txt = ui.text(text, ui.color('text'), ui.fonts.regular)

    btn:child(box)
    box:paddingleft(10)
    box:paddingright(10)
    box:paddingtop(5)
    box:paddingbottom(5)
    box:pushback(txt, 'middle', false)

    return btn
end

function M.wizard.start()
    local w = {}

    setmetatable(w, M.wizard)

    w.win = ui.window('Import TacO Pack')
    w.box = ui.box('vertical')
    w.msg = ui.text(table.concat({
        'This utility will convert a TacO/zip marker pack into the EG-Overlay',
        'marker package format.',
        '',
        "Select the TacO and destination marker package paths below, and then",
        "press Convert."
    }, '\n'), ui.color('text'), ui.fonts.regular)

    w.win:child(w.box)
    w.box:paddingleft(5)
    w.box:paddingright(5)
    w.box:paddingtop(5)
    w.box:paddingbottom(5)
    w.box:spacing(5)

    w.box:pushback(w.msg, 'start', false)

    w.box:pushback(ui.separator('horizontal'), 'fill', false)

    w.box:pushback(ui.text('Input TacO Marker Pack:', ui.color('text'), ui.fonts.regular), 'start', false)

    w.tacotxt = ui.text('', ui.color('text'), ui.fonts.regular)
    w.box:pushback(w.tacotxt, 'start', false)

    w.tacobtn = txtbutton('Select TacO Pack')
    w.box:pushback(w.tacobtn, 'fill', false)

    w.box:pushback(ui.separator('horizontal'), 'fill', false)

    w.box:pushback(ui.text('Output Marker Package:', ui.color('text'), ui.fonts.regular), 'start', false)

    w.dbtxt = ui.text('', ui.color('text'), ui.fonts.regular)
    w.box:pushback(w.dbtxt, 'start', false)

    w.dbbtn = txtbutton('Select Marker Package')
    w.box:pushback(w.dbbtn, 'fill', false)

    w.box:pushback(ui.separator('horizontal'), 'fill', false)

    w.convertbtn = txtbutton('Convert')
    w.box:pushback(w.convertbtn, 'fill', false)

    w.cancelbtn = txtbutton('Cancel')
    w.box:pushback(w.cancelbtn, 'fill', false)

    w.tacobtn:addeventhandler(function() w:onselecttaco() end, 'click-left')
    w.dbbtn:addeventhandler(function() w:onselectdb() end, 'click-left')
    w.convertbtn:addeventhandler(function() w:onconvert() end, 'click-left')
    w.cancelbtn:addeventhandler(function() w:oncancel() end, 'click-left')

    w.win:position(50, 50)
    w.win:show()

    return w
end

function M.wizard:oncancel()
    self.win:hide()
end

function M.wizard:onselecttaco()
    local d = dialogs.FileDialog.new('open-file')
    d.confirmcallback = function(path) self:ontacoselected(path) end
    d.filefilters = {'.taco','.zip'}

    if self.tacopath then
        d:gotopath(path.parent(self.tacopath))
    end

    d:show()
end

function M.wizard:ontacoselected(tacopath)
    self.tacopath = tacopath
    self.tacotxt:text(tacopath)

    self.dbpath = path.withextension(self.tacopath, 'db')
    self.dbtxt:text(self.dbpath)
end

function M.wizard:onselectdb()
    local d = dialogs.FileDialog.new('save-file')
    d.confirmcallback = function(path) self:ondbselected(path) end
    d.filefilters = { '.db' }

    if self.dbpath then
        d.selectedtxt:text(path.filename(self.dbpath))
        d:gotopath(path.parent(self.dbpath))
    end

    d:show()
end

function M.wizard:ondbselected(dbpath)
    self.dbpath = path.withextension(dbpath, 'db')
    self.dbtxt:text(self.dbpath)
end

function M.wizard:onconvert()
    self.win:hide()

    local win = ui.window('Converting TacO...')
    local box = ui.box('vertical')

    box:paddingleft(10)
    box:paddingtop(10)
    box:paddingright(10)
    box:paddingbottom(10)
    box:spacing(5)

    local msgbox = ui.box('horizontal')
    msgbox:spacing(10)

    msgbox:pushback(ui.text(ui.iconcodepoint('hourglass'), ui.color('text'), ui.fonts.icon:tosizeperc(2.5)), 'middle', false)
    box:pushback(msgbox, 'start', false)

    local msg = ui.text(table.concat({
        'The TacO pack is being converted, this may take a while...',
        '',
        'Progress is shown below, any warnings or errors will be',
        'recorded to the EG-Overlay log.',
    }, '\n'), ui.color('text'), ui.fonts.regular)
    msgbox:pushback(msg, 'middle', false)

    box:pushback(ui.separator('horizontal'), 'fill', false)

    win:child(box)

    win:position(100,100)
    win:show()

    box:pushback(ui.text(string.format("Starting conversion of %s...", self.tacopath), ui.color('text'), ui.fonts.regular),'start', false)

    local c = M.taco2db.new(self.tacopath, self.dbpath)

    local thread = coroutine.create(function() c:run() end)
    local last_stage = 'start'
    local last_txt = nil

    while coroutine.status(thread) ~= 'dead' do
        local r, stage, n, t = coroutine.resume(thread)

        if not r then
            overlay.logerror(string.format("Error during TacO conversion: %s", stage))
            local errtxt = ui.text(string.format("Error occured: %s", stage), ui.color('text'), ui.fonts.regular)
            box:pushback(errtxt, 'start', false)
            break
        end

        if not stage then break end

        if last_stage ~= stage then
            last_txt = ui.text(stage, ui.color('text'), ui.fonts.regular)
            box:pushback(last_txt, 'start', false)
            last_stage = stage
        end

        if stage and n and t then
            last_txt:text(string.format('%s %0.1f%% (%d/%d)', stage, (n/t)*100.0, n, t))
        end
        coroutine.yield()
    end

    box:pushback(ui.separator('horizontal'), 'fill', false)

    local btn = ui.button()
    local bbox = ui.box('vertical')
    local btxt = ui.text('OK', ui.color('text'), ui.fonts.regular)

    bbox:paddingleft(10)
    bbox:paddingright(10)
    bbox:paddingtop(5)
    bbox:paddingbottom(5)
    bbox:pushback(btxt, 'middle', false)
    btn:child(bbox)

    box:pushback(btn, 'fill', false)

    btn:addeventhandler(function() win:hide() end, 'click-left')
end

return M
