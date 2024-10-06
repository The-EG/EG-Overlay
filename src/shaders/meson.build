
shader_srcs = [
    'rect.frag',
    'rect.vert',
    'text-quad.frag',
    'text-quad.vert',
    'polyline.vert',
    'polyline.frag',
    'image.vert',
    'image.frag',

    '3dcommon.glsl',
    'trail.vert',
    'trail.frag',
    'sprite-array.vert',
    'sprite-array.frag',
]

shaders = []

foreach shader : shader_srcs
    shaders += fs.copyfile(shader, shader)
endforeach
