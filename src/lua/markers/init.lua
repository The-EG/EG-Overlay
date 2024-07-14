require 'mumble-link-events'

local overlay = require 'eg-overlay'
local loaders = require 'markers.loaders'
local logger = require 'logger'
local ml = require 'mumble-link'
local settings = require 'settings'

local ui = require 'eg-overlay-ui'
local uih = require 'ui-helpers'

local markers = {}

markers.settings = settings.new('markers.lua')

markers.log = logger.logger:new('markers')

markers.categories = {}

local function checkbox_menu_item(text)
    local mi = ui.menu_item()
    local settings = overlay.settings()
    local font_name = settings:get('overlay.ui.font.path')
    local font_size = settings:get('overlay.ui.font.size')
    local mit = ui.text(text, 0xFFFFFFFF, font_name, font_size)
    local mic = uih.checkbox()

    mi:set_child(mit)
    mi:set_pre(mic)
    return {menuitem = mi, checkbox = mic, text = mit}
end

local function menu_item(text)
    local mi = ui.menu_item()
    local settings = overlay.settings()
    local font_name = settings:get('overlay.ui.font.path')
    local font_size = settings:get('overlay.ui.font.size')
    local mit = ui.text(text, 0xFFFFFFFF, font_name, font_size)
    mi:set_child(mit)
    return { menuitem = mi, text = mit}
end

local function sep_menu_item()
    local sep = ui.separator('horizontal')
    local mi = ui.menu_item()
    mi:set_child(sep)

    return {menuitem = mi, separator = sep}
end

markers.main_menu = {
    menu = ui.menu(),
    show_markers = checkbox_menu_item("Show Markers"),
    manage_markers = menu_item("Manage Markers"),
    sep1 = sep_menu_item(),
    reload_categories = menu_item("Reload Categories"),
    settings = menu_item("Settings")
}

markers.main_menu.sep1.menuitem:enabled(false)

markers.main_menu.menu:add_item(markers.main_menu.show_markers.menuitem)
markers.main_menu.menu:add_item(markers.main_menu.manage_markers.menuitem)
markers.main_menu.menu:add_item(markers.main_menu.sep1.menuitem)
markers.main_menu.menu:add_item(markers.main_menu.reload_categories.menuitem)
markers.main_menu.menu:add_item(markers.main_menu.settings.menuitem)

function markers.on_map_change()
    markers.log:info("Map changed, reloading...")
    markers.pois_in_map = {}
    markers.categories_in_map = {}

    local start_time = overlay.time()

    local ids_in_map = {}
    for i, p in ipairs(markers.pois) do
        if tonumber(p.MapID)==ml.map_id then
            if p.type then ids_in_map[p.type] = true end
            table.insert(markers.pois_in_map, p)
        end
    end

    local end_time = overlay.time()

    markers.log:info("%d pois, took %.2f seconds", #markers.pois_in_map, (end_time - start_time))
    markers.log:info("  categories in map:")
    for k,v in pairs(ids_in_map) do
        markers.log:info("    %s", k)
    end
end

function markers.on_main_menu_event(event)
    if event=='click-left' then
        local x,y = ui.mouse_position()
        markers.log:debug("showing menu")
        markers.main_menu.menu:show(x,y)
    end
end

local function on_category_menu_click(category)
    markers.log:debug("Category clicked: %s | %s -> %s", category:gettypeid(), category.active, not category.active)
    category.active = not category.active
    markers.category_settings[category.gettypeid()] = active
    markers.settings.save()

    return true
end

local function on_category_checkbox_event(category, event)
    if event ~= 'toggle-on' and event ~= 'toggle-off' then return end

    markers.log:debug('%s - %s (%s)', category:get_type_id(), event, category.active)
end

-- function markers.build_category_menu(category)
--     if not category.properties.name then return end
--     local mi = ui.menu_item()
--     local settings = overlay.settings()
--     local font_name = settings:get('overlay.ui.font.pathMono')
--     local font_size = settings:get('overlay.ui.font.size')
--     local mit = ui.text(category.properties.displayname or category.properties.name, 0xFFFFFFFF, font_name, font_size)
    

--     if category.isseparator then
--         local sep1 = ui.separator('horizontal')
--         local sep2 = ui.separator('horizontal')
--         local mibox = ui.box('vertical')
--         mibox:pack_end(sep1, false, 'fill')
--         if category.properties.displayname and string.sub(category.properties.displayname,1,1)==' ' then
--             -- some marker pack devs try to center their separator categories
--             -- and they'll look bad if they are centered with all the spaces before
--             -- actual text
--             mibox:pack_end(mit, false, 'start')
--         else
--             -- others don't and they'll look quite nice centered
--             mibox:pack_end(mit, false, 'middle')
--         end
--         mibox:pack_end(sep2, false, 'fill')
--         mi:set_child(mibox)
--     else
--         local mic = uih.checkbox()
--         mi:set_child(mit)
--         mi:set_pre(mic)
--         mic:bind_value(category, 'active')
--         mic:event_handler(function(event)
--             on_category_checkbox_event(category, event)
--         end)
--     end

--     if #category.children > 0 then
--         local child_menu = ui.menu()
--         mi:set_submenu(child_menu)
--         for i,c in ipairs(category.children) do
--             local cmi = markers.build_category_menu(c)
--             if cmi then 
--                 child_menu:add_item(cmi)
--             end
--         end
--     end

--     if mi then
--         mi:on_click(function()
--             return on_category_menu_click(category)
--         end)
--     end

--     return mi
-- end

function markers.on_startup()
    overlay.queue_event('register-module-actions', {
        name = "Markers",
        primary_action = markers.on_main_menu_event
    })

    -- local marker_packs = {
    --     "D:\\Downloads\\GW2 TacO ReActif EN External.taco",
    --     "D:\\Downloads\\TehsTrails.taco",
    --     "D:\\Downloads\\tw_ALL_IN_ONE.taco"
    -- }

    -- markers.all_menu = ui.menu()



    -- for i,mp in ipairs(marker_packs) do
    --     local zl = loaders.zip_loader:new(mp)
    --     zl:load()
    -- end

    -- markers.log:debug("Building category menus...")
    -- for i,l in ipairs(markers.loaders) do
    --     for i,c in ipairs(l.root_category.children) do
    --         --setup_category_settings(c)
    --         local mi = markers.build_category_menu(c)
    --         if mi then 
    --             markers.all_menu:add_item(mi)
    --         end
    --     end
    -- end

end

overlay.add_event_handler('startup', markers.on_startup)

return markers