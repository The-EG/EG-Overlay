#version 460
#extension GL_ARB_shading_language_include : require

#define FRAGMENT_SHADER
#include "/3dcommon.glsl"

layout(location = 0) in vec2 texpos;
layout(location = 1) in float fade_dist;
layout(location = 2) in float cam_player_dist;
layout(location = 3) in float vert_cam_dist;

layout(location = 2) uniform float fade_near;
layout(location = 3) uniform float fade_far;
layout(location = 4) uniform vec4 color;
layout(location = 5) uniform float map_left;
layout(location = 6) uniform float map_bottom;
layout(location = 7) uniform float map_height;
layout(location = 8) uniform uint inmap;

layout(location = 0) out vec4 outColor;

uniform sampler2D texSampler;

void main() {
    if (inmap==0) discard_if_in_map(map_left, map_bottom, map_height);

    float alpha = color.a;

    // fade based on distance from camera
    if (inmap==0) {
        alpha = min(alpha, distance_fade_alpha(fade_near, fade_far, fade_dist));
        if (alpha < 0.01) discard;
        

        if (cam_player_dist >= vert_cam_dist) {
            alpha = min(0.05, alpha);
        } else if (vert_cam_dist - cam_player_dist <= 300) {
            float adist = vert_cam_dist - cam_player_dist;
            float a = ((adist / 300) * (1.0 - 0.05)) + 0.05;
            alpha = min(alpha, a);
        }
   }
    
    vec4 texcolor = texture(texSampler, texpos);
    
    alpha *= texcolor.a;

    if (alpha < 0.01) discard;

    outColor = vec4((texcolor.rgb * color.rgb) * alpha, alpha);
}
