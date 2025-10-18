# EG-Overlay
# Copyright (c) 2025 Taylor Talkington
# SPDX-License-Identifier: MIT

configure_file(input: 'eg-overlay.h.in', output: 'eg-overlay.h', configuration: version_data)
version_rs = configure_file(input: 'version.rs.in', output: 'version.rs', configuration: version_data)
githash_rs = vcs_tag(input: 'githash.rs.in', output: 'githash.rs', fallback: 'unknown')


subdir('shaders')
subdir('lua')
subdir('fonts')
subdir('res')
subdir('textures')

exe = executable(
    'eg-overlay',
#    'main.rs',
    structured_sources(
        [
            'main.rs',
            version_rs,
            githash_rs,
            'dx.rs',
            'ft.rs',
            'input.rs',
            'lamath.rs',
            'logging.rs',
            'lua.rs',
            'lua_manager.rs',
            'lua_json.rs',
            'lua_path.rs',
            'lua_shell.rs',
            'lua_sqlite3.rs',
            'ml.rs',
            'overlay.rs',
            'settings.rs',
            'ui.rs',
            'utils.rs',
            'web_request.rs',
            'zip.rs',
        ],
        {
            'dx': ['dx/lua.rs'],
            'logging': ['logging/sinks.rs'],
            'ml': ['ml/lua.rs'],
            'overlay': ['overlay/lua.rs'],
            'settings': ['settings/lua.rs'],
            'ui': [
                'ui/button.rs',
                'ui/entry.rs',
                'ui/font.rs',
                'ui/grid.rs',
                'ui/lua.rs',
                'ui/rect.rs',
                'ui/menu.rs',
                'ui/scrollview.rs',
                'ui/separator.rs',
                'ui/text.rs',
                'ui/uibox.rs',
                'ui/window.rs',
            ],
            'ui/button': ['ui/button/lua.rs'],
            'ui/entry': ['ui/entry/lua.rs'],
            'ui/font': ['ui/font/lua.rs'],
            'ui/grid': ['ui/grid/lua.rs'],
            'ui/menu': ['ui/menu/lua.rs'],
            'ui/scrollview': ['ui/scrollview/lua.rs'],
            'ui/separator': ['ui/separator/lua.rs'],
            'ui/text': ['ui/text/lua.rs'],
            'ui/uibox': ['ui/uibox/lua.rs'],
            'ui/window': ['ui/window/lua.rs'],
            'zip': ['zip/lua.rs'],
        }
    ),
    dependencies: [windows_rs_dep, serde_json_rs, serde_rs, xml_rs, lua_dep, zlib_dep, freetype2_dep, sqlite_dep],

    # Link windows RC resources in. This is ugly, but maybe meson will have better support in the future...
    rust_args: [
        '-C','link-arg=@0@'.format(icons_res.full_path()),
        '-C','link-arg=@0@'.format(version_res.full_path()),
    ],
)

