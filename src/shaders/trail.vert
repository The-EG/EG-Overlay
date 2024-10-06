#version 460

layout(location = 0) in vec3 pos;
layout(location = 1) in vec2 texuv;

layout(location = 0) uniform mat4 view;
layout(location = 1) uniform mat4 proj;
layout(location = 8) uniform vec3 player_pos;
layout(location = 12) uniform uint inmap;

layout(location = 0) out vec2 texpos;
layout(location = 1) out float fade_dist;

void main() {
    gl_Position = proj * view * vec4(pos, 1.0);
    texpos = texuv;
    
    if (inmap==0) fade_dist = distance(player_pos, pos);
    else fade_dist = 0.0;
}

