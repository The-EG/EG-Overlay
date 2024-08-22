windows = import('windows')

icon_res = windows.compile_resources(
    'icons.rc',
    'version.rc',
    args:['/UNICODE'],
    include_directories: [meson.project_build_root() + '/src']
)