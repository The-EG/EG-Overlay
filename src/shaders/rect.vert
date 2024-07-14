#version 460

layout(location = 0) uniform float left;
layout(location = 1) uniform float top;
layout(location = 2) uniform float right;
layout(location = 3) uniform float bottom;
layout(location = 4) uniform vec4 color;
layout(location = 5) uniform mat4 proj;

layout(location = 0) out vec4 fragColor;

void main() {
    switch (gl_VertexID) {
    case 0: 
        gl_Position = proj * vec4(left, top, 0.0, 1.0);
        break;
    case 4:
    case 1:
        gl_Position = proj * vec4(left, bottom, 0.0, 1.0);
        break;
    case 3:
    case 2: 
        gl_Position = proj * vec4(right, top, 0.0, 1.0);
        break;
    case 5:
        gl_Position = proj * vec4(right, bottom, 0.0, 1.0);
        break;
    default: 
        gl_Position = proj * vec4(0.0, 0.0, 0.0, 1.0);
        break;
    }

    fragColor = color;
}