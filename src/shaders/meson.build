# EG-Overlay
# Copyright (c) 2025 Taylor Talkington
# SPDX-License-Identifier: MIT
rootsig = 'root-sig.hlsl'

projopt = get_option('optimization')

extra_dxc_opts = []

if projopt in ['0','1','2','3']
    extra_dxc_opts += '-O' + projopt
elif projopt in ['g']
    extra_dxc_opts += '-Od'
else
    extra_dxc_opts += '-O3'
endif

if get_option('debug')
    extra_dxc_opts += '-Zi'
    extra_dxc_opts += '-Qembed_debug'
endif

shaders = []

shaders += custom_target(
    rootsig,
    build_by_default: true,
    input: rootsig,
    output: rootsig.replace('.hlsl','.cso'),
    command: [dxc, '-T','rootsig_1_1', '-Zi', '-E', 'ROOTSIG','-Fo','@OUTDIR@/@BASENAME@.cso','@INPUT@'] + extra_dxc_opts,
)

hlsl_srcs = [
    {'source': 'rect.vs.hlsl', 'profile': 'vs_6_1', 'includes': ['rect.hlsl']},
    {'source': 'rect.ps.hlsl', 'profile': 'ps_6_1', 'includes': ['rect.hlsl']},

    {'source': 'font-quad.vs.hlsl', 'profile': 'vs_6_1', 'includes': ['font-quad.hlsl']},
    {'source': 'font-quad.ps.hlsl', 'profile': 'ps_6_1', 'includes': ['font-quad.hlsl']},

    {'source': 'sprite-list.vs.hlsl', 'profile': 'vs_6_1', 'includes': ['sprite-list.hlsl','3dcommon.hlsl']},
    {'source': 'sprite-list.ps.hlsl', 'profile': 'ps_6_1', 'includes': ['sprite-list.hlsl','3dcommon.hlsl']},

    {'source': 'trail.vs.hlsl', 'profile': 'vs_6_1', 'includes': ['trail.hlsl', '3dcommon.hlsl']},
    {'source': 'trail.ps.hlsl', 'profile': 'ps_6_1', 'includes': ['trail.hlsl', '3dcommon.hlsl']},

    {'source': 'image.vs.hlsl', 'profile': 'vs_6_1', 'includes': ['image.hlsl']},
    {'source': 'image.ps.hlsl', 'profile': 'ps_6_1', 'includes': ['image.hlsl']},
]


foreach hlsl : hlsl_srcs
    src = hlsl['source']
    profile = hlsl['profile']
    shaders += custom_target(
        src,
        build_by_default: true,
        input: src,
        output: [src.replace('.hlsl', '.cso')],
        depend_files: hlsl['includes'],
        command: [dxc, '-T', profile, '-Fo', '@OUTDIR@/@BASENAME@.cso', '@INPUT@'] + extra_dxc_opts,
    )
endforeach

