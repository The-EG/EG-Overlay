#version 460

layout(location = 1) uniform vec4 color;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = color;
}