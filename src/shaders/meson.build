
shader_srcs = [
    'rect.frag',
    'rect.vert',
    'text-quad.frag',
    'text-quad.vert',
    'polyline.vert',
    'polyline.frag',
    'image.vert',
    'image.frag',
]

shaders = []

foreach shader : shader_srcs
    shaders += fs.copyfile(shader, shader)
endforeach