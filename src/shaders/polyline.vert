#version 460
layout(location = 0) in vec2 in_position;

layout(location = 0) uniform mat4 proj;

void main() {
    gl_Position = proj * vec4(in_position, 0.0, 1.0);
}