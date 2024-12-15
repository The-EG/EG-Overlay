rootsig = 'root-sig.hlsl'

shaders = []

shaders += custom_target(
    rootsig,
    input: rootsig,
    output: rootsig.replace('.hlsl','.cso'),
    command: [dxc, '-T','rootsig_1_1', '-E', 'ROOTSIG','-Fo','@OUTDIR@/@BASENAME@.cso','@INPUT@'],
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

