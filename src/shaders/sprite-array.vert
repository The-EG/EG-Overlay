#version 460
#extension GL_ARB_shading_language_include : require

#include "/3dcommon.glsl"

layout(location = 0) in vec3 pos;
layout(location = 1) in float max_u;
layout(location = 2) in float max_v;
layout(location = 3) in float xy_ratio;
layout(location = 4) in float size;
layout(location = 5) in float fade_near;
layout(location = 6) in float fade_far;
layout(location = 7) in vec4 color;
layout(location = 8) in uint flags;
layout(location = 9) in mat4 rotation;

layout(location = 0) uniform mat4 view;
layout(location = 1) uniform mat4 proj;
layout(location = 2) uniform vec3 player_pos;
layout(location = 9) uniform uint ismap;

layout(location = 0) out vec2 frag_tex_coord;
layout(location = 1) out vec4 frag_color;
layout(location = 2) flat out uint out_flags;
layout(location = 3) out float fade_alpha; // result of fading due to fade_near / fade_far

void main() {
    float y_size = size;
    float x_size = y_size * xy_ratio;

    mat3 billboard = mat3(
        view[0].xyz,
        view[1].xyz,
        view[2].xyz
    );

    float left = x_size / 2.0 * -1.0;
    float right = x_size / 2.0;
    float top = y_size / 2.0 * (ismap > 0 ? -1.0 : 1.0);
    float bottom = y_size / 2.0 * (ismap > 0 ? 1.0 : -1.0);

    vec3 vpos = vec3(0,0,0);
    switch (gl_VertexID) {
    //  C----D
    //  |    |
    //  A----B
    // triangle strip, triangle 1 = b,a,d; triangle 2 = d,a,c
    case 0: // B
        vpos = vec3(right, bottom, 0.0);
        frag_tex_coord = vec2(max_u, 0.0);
        break;
    case 1: // A
        vpos = vec3(left, bottom, 0.0);
        frag_tex_coord = vec2(0.0, 0.0);
        break;
    case 2: // D
        vpos = vec3(right, top, 0.0);
        frag_tex_coord = vec2(max_u, max_v);
        break;
    case 3: // C
        vpos = vec3(left, top, 0.0);
        frag_tex_coord = vec2(0.0, max_v);
        break;
    }

    if (ismap==0) {
        if ((flags & BILLBOARD) > 0) {
            vpos *= billboard;
        } else {
            vpos = (vec4(vpos, 1.0) * rotation).xyz;
        }
    }

    out_flags = flags;

    gl_Position = proj * view * vec4(pos.xyz + vpos, 1.0);
    frag_color = color;

    if (ismap==0) {
        float fade_dist = distance(player_pos, pos);
        fade_alpha = distance_fade_alpha(fade_near, fade_far, fade_dist);
    } else {
        fade_alpha = 1.0;
    }
}
