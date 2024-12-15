#include "sprite-list.hlsl"

#define PIXEL_SHADER
#include "3dcommon.hlsl"

Texture2D    texture : register(t0);
SamplerState texsampler : register(s0);

float4 main(PSInput input) :SV_Target {
    
    if (ismap==0) {
        discard_if_in_map(input.position, map_left, map_top, map_height);
        if (input.fade_alpha < 0.01) discard;
    }
    

    float4 texcolor = texture.Sample(texsampler, input.texuv);
    
    float alpha = texcolor.a;

    if (ismap==0) {
        alpha = min(alpha, input.fade_alpha);
        if (input.cam_player_dist >= input.vert_cam_dist) {
            alpha = min(alpha, 0.05);
        } else if (input.vert_cam_dist - input.cam_player_dist <= 300) {
            float adist = input.vert_cam_dist - input.cam_player_dist;
            float a = ((adist / 300) * (1.0 - 0.05)) + 0.05;
            alpha = min(alpha, a);
        }
    }

    alpha *= texcolor.a;

    if (alpha < 0.01) discard;

    return float4((texcolor.rgb * input.color.rgb) * alpha, alpha);
}
