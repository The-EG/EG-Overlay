// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT
#define PIXEL_SHADER
#include "trail.hlsl"
#include "3dcommon.hlsl"

Texture2D    texture    : register(t0);
SamplerState texsampler : register(s0);

float4 main(PSInput input) : SV_Target {
    if (inmap==0) discard_if_in_map(input.position, map_left, map_top, map_height);

    float alpha = color.a;

    if (inmap==0) {
        float fade_dist = distance(player_pos, input.trail_pos);
        alpha = min(alpha, distance_fade_alpha(fade_near, fade_far, fade_dist));
        if (alpha < 0.01) discard;

        float vertcamdist = distance(camera_pos, input.trail_pos);

        if (input.cam_player_dist >= vertcamdist) {
            alpha = min(0.05, alpha);
        } else if (vertcamdist - input.cam_player_dist <= 36) {
            float adist = vertcamdist - input.cam_player_dist;
            float a = ((adist / 36) * (1.0 - 0.05)) + 0.05;
            alpha = min(alpha, a);
        }

        /*
        float adist = input.cam_player_dist - vertcamdist;

        if (adist > 12.0) {
            alpha = min(0.05, alpha);
        } else if (adist <= 12.0 && adist > 0.0) {
            float a = 1.0 - ((adist / 12.0) * (1.0-0.05));
            alpha = min(alpha, a);
        }
        */
    }

    float4 texcolor = texture.Sample(texsampler, input.texuv);

    alpha *= texcolor.a;

    if (alpha < 0.01) discard;

    return float4((texcolor.rgb * color.rgb) * alpha, alpha);
}
