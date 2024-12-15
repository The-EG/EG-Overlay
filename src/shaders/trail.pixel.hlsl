#define FRAGMENT_SHADER
#include "trail.hlsl"
#include "3dcommon.hlsl"

Texture2D    texture    : register(t0);
SamplerState texsampler : register(s0);

float4 main(PSInput input) : SV_Target {
    if (inmap==0) discard_if_in_map(input.position, map_left, map_top, map_height);

    float alpha = color.a;

    if (inmap==0) {
        alpha = min(alpha, distance_fade_alpha(fade_near, fade_far, input.fade_dist));
        if (alpha < 0.01) discard;

        if (input.cam_player_dist >= input.vert_cam_dist) {
            alpha = min(0.05, alpha);
        } else if (input.vert_cam_dist - input.cam_player_dist <= 300) {
            float adist = input.vert_cam_dist - input.cam_player_dist;
            float a = ((adist / 300) * (1.0 - 0.05)) + 0.05;
            alpha = min(alpha, a);
        }
    }

    float4 texcolor = texture.Sample(texsampler, input.texuv);

    alpha *= texcolor.a;

    if (alpha < 0.01) discard;

    return float4((texcolor.rgb * color.rgb) * alpha, alpha);
}
