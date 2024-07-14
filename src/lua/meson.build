lua_srcs = [
    'autoload.lua',
    'main-menu.lua',
    'console.lua',
    'overlay-stats.lua',
    'mumble-link-info.lua',
    'mumble-link-events.lua',
    'map-buddy.lua',
    'logger.lua',
    'ui-helpers.lua',
    'db.lua',
    'psna-tracker.lua'
]

lua_files = []

foreach lua : lua_srcs
    lua_files += fs.copyfile(lua, lua)
endforeach

subdir('gw2')
subdir('markers')