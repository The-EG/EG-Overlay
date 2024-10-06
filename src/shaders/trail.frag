#version 460
#extension GL_ARB_shading_language_include : require

#define FRAGMENT_SHADER
#include "/3dcommon.glsl"

layout(location = 0) in vec2 texpos;
layout(location = 1) in float fade_dist;

layout(location =  2) uniform float fade_near;
layout(location =  3) uniform float fade_far;
layout(location =  4) uniform vec2 fade_region_center;
layout(location =  5) uniform float fade_region_r;
layout(location =  6) uniform float fade_region_fr;
layout(location =  7) uniform vec4 color;
layout(location =  9) uniform float map_left;
layout(location = 10) uniform float map_bottom;
layout(location = 11) uniform float map_height;
layout(location = 12) uniform uint inmap;

layout(location = 0) out vec4 outColor;

uniform sampler2D texSampler;

void main() {
    if (inmap==0) discard_if_in_map(map_left, map_bottom, map_height);

    float alpha = color.a;

    // fade based on distance from camera
    if (inmap==0) {
        alpha = min(alpha, distance_fade_alpha(fade_near, fade_far, fade_dist));
        if (alpha < 0.01) discard;
        

       // fade based on position on screen
        alpha = min(alpha, fade_region_alpha(fade_region_center, fade_region_r, fade_region_fr));
    }
    
    vec4 texcolor = texture(texSampler, texpos);
    
    alpha *= texcolor.a;

    if (alpha < 0.01) discard;

    outColor = vec4((texcolor.rgb * color.rgb) * alpha, alpha);
}
