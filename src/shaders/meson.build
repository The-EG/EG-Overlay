rootsig = 'root-sig.hlsl'

shaders = []

shaders += custom_target(
    rootsig,
    input: rootsig,
    output: rootsig.replace('.hlsl','.cso'),
    command: [dxc, '-T','rootsig_1_1', '-E', 'ROOTSIG','-Fo','@OUTDIR@/@BASENAME@.cso','@INPUT@'],
)

hlsl_srcs = [
    {'source': 'rect.vert.hlsl', 'profile': 'vs_6_1', 'includes': ['rect.root.hlsl']},
    {'source': 'rect.pixel.hlsl', 'profile': 'ps_6_1', 'includes': ['rect.root.hlsl']},
    
    {'source': 'font-quad.vert.hlsl', 'profile': 'vs_6_1', 'includes': ['font-quad.root.hlsl']},
    {'source': 'font-quad.pixel.hlsl', 'profile': 'ps_6_1', 'includes': ['font-quad.root.hlsl']},

    {'source': 'sprite-list.vert.hlsl', 'profile': 'vs_6_1', 'includes': ['sprite-list.hlsl','3dcommon.hlsl']},
    {'source': 'sprite-list.pixel.hlsl', 'profile': 'ps_6_1', 'includes': ['sprite-list.hlsl','3dcommon.hlsl']},

    {'source': 'trail.vert.hlsl', 'profile': 'vs_6_1', 'includes': ['trail.hlsl', '3dcommon.hlsl']},
    {'source': 'trail.pixel.hlsl', 'profile': 'ps_6_1', 'includes': ['trail.hlsl', '3dcommon.hlsl']},
]


foreach hlsl : hlsl_srcs
    src = hlsl['source']
    profile = hlsl['profile']
    shaders += custom_target(
        src,
        input: src,
        output: [src.replace('.hlsl', '.cso')], 
        depend_files: hlsl['includes'],
        command: [dxc, '-T', profile, '-Fo', '@OUTDIR@/@BASENAME@.cso', '@INPUT@'],
    )
endforeach

