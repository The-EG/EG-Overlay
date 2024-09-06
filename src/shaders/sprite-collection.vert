#version 460

layout(location = 0) in vec3 pos;
layout(location = 1) in float size_ratio;
layout(location = 2) in vec4 color;
layout(location = 3) in uint flags;
layout(location = 4) in mat4 rotation;

layout(location = 0) uniform float size;
layout(location = 1) uniform float xy_ratio;
layout(location = 2) uniform float max_u;
layout(location = 3) uniform float max_v;
layout(location = 4) uniform mat4 view;
layout(location = 5) uniform mat4 proj;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec4 fragColor;
layout(location = 2) flat out uint out_flags;

#define BILLBOARD (1u)

void main() {
    float y_size = size * size_ratio;
    float x_size = y_size * xy_ratio;

    mat3 billboard = mat3(
        view[0].xyz,
        view[1].xyz,
        view[2].xyz
    );

    float left = x_size / 2.0 * -1.0;
    float right = x_size / 2.0;
    float top = y_size / 2.0;
    float bottom = y_size / 2.0 * -1.0;

    vec3 vpos = vec3(0,0,0);
    switch (gl_VertexID) {
    case 0: 
        vpos = vec3(left, top, 0.0);
        fragTexCoord = vec2(0.0, 0.0);
        break;
    case 4:
    case 1:
        vpos = vec3(right, top, 0.0);
        fragTexCoord = vec2(max_u, 0.0);
        break;
    case 3:
    case 2: 
        vpos = vec3(left, bottom, 0.0);
        fragTexCoord = vec2(0.0, max_v);
        break;
    case 5:
        vpos = vec3(right, bottom, 0.0);
        fragTexCoord = vec2(max_u, max_v);
        break;
    }

    if ((flags & BILLBOARD) > 0) {
        vpos *= billboard;
    } else {
        vpos = (vec4(vpos, 1.0) * rotation).xyz;
    }

    out_flags = flags;

    gl_Position = proj * view * vec4(pos.xyz + vpos, 1.0);
    fragColor = color;
}