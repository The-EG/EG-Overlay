pathing_lua_srcs = [
    'init.lua',
    'converters.lua',
    'loaders.lua',
    'package.lua',
    'menu.lua',
    'settings.lua',
    'manager.lua',
    'category-manager.lua',
]

foreach lua : pathing_lua_srcs
    lua_files += fs.copyfile(lua, lua)
endforeach
