# EG-Overlay
# Copyright (c) 2025 Taylor Talkington
# SPDX-License-Identifier: MIT

pathing_lua_srcs = [
    'init.lua',
    'package.lua',
    'menu.lua',
    'settings.lua',
    'manager.lua',
    'category-manager.lua',
    'data.lua',
    'taco2db.lua',
]

foreach lua : pathing_lua_srcs
    lua_files += fs.copyfile(lua, lua)
endforeach
