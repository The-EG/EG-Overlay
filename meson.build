# EG-Overlay
# Copyright (c) 2025 Taylor Talkington
# SPDX-License-Identifier: MIT
project('eg-overlay', 'rust', 'c', meson_version: '>=1.3.0', version: '0.3.0-dev', default_options:['rust_std=2024'])

dxc = find_program('dxc')
pwsh = find_program('pwsh')
rc = find_program('rc')

fs = import('fs')

version_data = configuration_data()
version_data.set('version_str', meson.project_version())
version_data.set('rc_version_num', '0,3,0,0')

windows_rs_dep = dependency('windows-0.61-rs', default_options:['rust_std=2021'])
serde_rs = dependency('serde-1-rs', default_options:['rust_std=2018'])
serde_json_rs = dependency('serde_json-1-rs', default_options:['rust_std=2021'])
xml_rs = dependency('xml-0.8-rs', default_options:['rust_std=2021'])

lua_proj = subproject('lua-5.4.7')
lua_dep = lua_proj.get_variable('lua_dep')

zlib_dep = dependency('zlib')
freetype2_dep = dependency('freetype2')

sqlite_proj = subproject('sqlite-amalgamation-3460000')
sqlite_dep = sqlite_proj.get_variable('sqlite_dep')

subdir('src')
