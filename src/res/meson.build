# EG-Overlay
# Copyright (c) 2025 Taylor Talkington
# SPDX-License-Identifier: MIT

icons_res = custom_target(
    'icons.res',
    build_by_default: true,
    input: ['icons.rc'],
    output: ['icons.res'],
    command: [rc, '/nologo','/fo@OUTPUT@','@INPUT@'],
)

version_res = custom_target(
    'version.res',
    build_by_default: true,
    input: ['version.rc'],
    output: ['version.res'],
    command: [rc, '/i@0@/src'.format(meson.project_build_root()),'/nologo','/fo@OUTPUT@','@INPUT@'],
)

