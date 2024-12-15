#include "trail.hlsl"
#include "3dcommon.hlsl"

struct VSInput {
    float3 position : POSITION;
    float2 texuv    : TEXUV;
};

PSInput main(VSInput input) {
    PSInput output;

    output.position = mul(proj, mul(view, float4(input.position, 1.0)));
    output.texuv    = input.texuv;

    output.cam_player_dist = distance(camera_pos, player_pos);
    output.vert_cam_dist   = distance(camera_pos, input.position);

    if (inmap==0) {
        output.fade_dist = distance(player_pos, input.position);
    } else {
        output.fade_dist = 0.0;
    }

    output.trail_pos = input.position;

    return output;
}
