# EG-Overlay
# Copyright (c) 2025 Taylor Talkington
# SPDX-License-Identifier: MIT

lua_srcs = [
    'autoload.lua',

    'overlay-menu.lua',

    'utils.lua',
    'mumble-link-events.lua',
    'dialogs.lua',

    'console.lua',

    'overlay-stats.lua',

    'psna-tracker.lua',

    'mumble-link-info.lua',
]

lua_files = []

foreach lua : lua_srcs
    lua_files += fs.copyfile(lua, lua)
endforeach


subdir('gw2')
subdir('markers')
