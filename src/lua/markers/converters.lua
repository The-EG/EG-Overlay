local converters = {}

local logger = require 'logger'

converters.log = logger.logger:new('markers.converters')

local function strtoint(str)
    local num = tonumber(str)

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
        converters.log:error("Invalid color string: %s", color);
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

-- Attribute list       Category        Markers         Trails
-- achievementid                        X               X
-- achievemtnbit                        X               X
-- alpha                                X               X
-- animspeed                                            X
-- autotrigger                          X
-- behavior                             X
-- bounce                               X
-- bounce-height                        X
-- bounce-duration                      X
-- bounce-delay                         X
-- canfade                              X               X
-- color                                X               X
-- copy                 X               X
-- cull                                 X               X
-- defaulttoggle        X
-- displayname          X
-- fadenear                             X               X
-- fadefar                              X               X
-- festival                             X               X
-- guid                                 X               X
-- heightoffset                         X               X
-- hide                                 X
-- iconfile                             X
-- iconsize                             X
-- info                                 X
-- inforange                            X
-- ingamevisibility                     X               X
-- invertbehavior                       X
-- ishidden            X
-- isseparator         X
-- iswall                                               X
-- mapdisplaysize                       X
-- mapid                                X
-- maptype                              X               X
-- mapvisibility                        X               X
-- minimapvisibility                    X               X
-- minsize                              X
-- maxsize                              X
-- mount                                X               X
-- name                X
-- occlude                              X
-- xpos                                 X
-- ypos                                 X
-- zpos                                 X
-- profession                           X               X
-- race                                 X               X
-- raid                                 X               X
-- resetguid                            X
-- resetlength                          X
-- rotate                               X
-- rotate-x                             X
-- rotate-y                             X
-- rotate-z                             X
-- scaleonmapwithzoom                   X
-- schedule                             X               X
-- script-tick                          X
-- script-focus                         X
-- script-trigger                       X
-- script-filter                        X
-- script-once                          X
-- show                                 X
-- specialization                       X               X
-- texture                                              X
-- tip-name            X                X
-- tip-description     X                X
-- toggle                               X
-- togglecategory                       X
-- traildata                                            X
-- trailscale                                           X
-- triggerrange                         X
-- type                                 X               X
-- 

converters.fromxml = {
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
    --['festival'          ] = strtotable,
    ['festival'          ] = unchanged,
    ['guid'              ] = unchanged,
    ['hascountdown'      ] = strtoint,
    ['heightoffset'      ] = strtofloat,
    ['hide'              ] = unchanged,
    ['iconfile'          ] = unchanged,
    ['iconsize'          ] = unchanged,
    ['info'              ] = unchanged,
    ['inforange'         ] = strtofloat,
    ['ingamevisibility'  ] = strtoint,
    ['invertbehavior'    ] = strotint,
    ['ishidden'          ] = strtoint,
    ['isseparator'       ] = strtoint,
    ['iswall'            ] = strtoint,
    ['mapdisplaysize'    ] = strtofloat,
    ['mapid'             ] = strtoint,
    --['maptype'           ] = strtotable,
    ['maptype'           ] = unchanged,
    ['mapvisibility'     ] = strtoint,
    ['minimapvisibility' ] = strtoint,
    ['minsize'           ] = strtofloat,
    ['maxsize'           ] = strtofloat,
    --['mount'             ] = strtotable,
    ['mount'             ] = unchanged,
    ['name'              ] = unchanged,
    ['occlude'           ] = strtoint,
    ['xpos'              ] = strtofloat,
    ['ypos'              ] = strtofloat,
    ['zpos'              ] = strtofloat,
    --['profession'        ] = strtotable,
    --['race'              ] = strtotable,
    --['raid'              ] = strtotable,
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
    --['specialization'    ] = strtotable,
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

return converters
