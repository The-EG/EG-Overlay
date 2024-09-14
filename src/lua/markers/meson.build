pathing_lua_srcs = [
    'init.lua',
    'converters.lua',
    'loaders.lua',
    'data.lua',
    'manager.lua',
]

foreach lua : pathing_lua_srcs
    lua_files += fs.copyfile(lua, lua)
endforeach
