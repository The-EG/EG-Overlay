#version 460
#extension GL_ARB_shading_language_include : require

#define FRAGMENT_SHADER
#include "/3dcommon.glsl"

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 1) in vec4 frag_color;
layout(location = 2) flat in uint flags;
layout(location = 3) in float fade_alpha;

layout(location = 0) out vec4 out_color;

layout(location = 3) uniform vec2 fade_region_center;
layout(location = 4) uniform float fade_region_r;
layout(location = 5) uniform float fade_region_fr;
layout(location = 6) uniform float map_left;
layout(location = 7) uniform float map_bottom;
layout(location = 8) uniform float map_height;
layout(location = 9) uniform uint ismap;

layout(binding = 0) uniform sampler2D tex_sampler;

void main() {
    //out_color = vec4(1.0,frag_tex_coord.x,0.0,1.0);

    //return;
    ///*

    if (ismap==0) {
        discard_if_in_map(map_left, map_bottom, map_height);
        // early out if it's faded out completely
        if (fade_alpha < 0.01) discard;
    }

    vec4 texcolor = texture(tex_sampler, frag_tex_coord);

    float alpha = frag_color.a;

    if (ismap==0) {
        alpha = min(alpha, fade_region_alpha(fade_region_center, fade_region_r, fade_region_fr));
        alpha = min(alpha, fade_alpha);
    }

    alpha *= texcolor.a;

    // a bit of a hack, but this keeps the transparent areas of sprites rendered
    // after others from occluding the non transparent sprites behind them
    // without this the order that sprites are rendered (declared) matters
    if (alpha < 0.01) discard;

    out_color = vec4((texcolor.rgb * frag_color.rgb) * alpha, alpha);
    //*/
}