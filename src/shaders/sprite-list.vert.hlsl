#include "sprite-list.hlsl"
#include "3dcommon.hlsl"

struct VSInput {
    float3   pos       : POSITION;
    float    max_u     : MAX_U;
    float    max_v     : MAX_V;
    float    xy_ratio  : XY_RATIO;
    float    size      : SIZE;
    float    fade_near : FADE_NEAR;
    float    fade_far  : FADE_FAR;
    float4   color     : COLOR;
    uint     flags     : FLAGS;
    float4x4 rotation  : ROTATION;
};

PSInput main(VSInput input, uint vert : SV_VertexID) {
    PSInput output;

    float y_size = input.size;
    float x_size = y_size * input.xy_ratio;

    float3x3 billboard = float3x3(
        view[0].xyz,
        view[1].xyz,
        view[2].xyz
    );

    float left = x_size / 2.0 * -1.0;
    float right = x_size / 2.0;
    float top = y_size / 2.0 * (ismap==0 ? -1.0 : 1.0);
    float bottom = y_size / 2.0 * (ismap==0 ? 1.0 : -1.0);

    float3 vpos;
    switch(vert) {
    case 0:
        vpos = float3(right, bottom, 0.0);
        output.texuv = float2(input.max_u, 0.0);
        break;
    case 1:
        vpos = float3(left, bottom, 0.0);
        output.texuv = float2(0.0, 0.0);
        break;
    case 2:
        vpos = float3(right, top, 0.0);
        output.texuv = float2(input.max_u, input.max_v);
        break;
    case 3:
        vpos = float3(left, top, 0.0);
        output.texuv = float2(0.0, input.max_v);
        break;
    }

    if (ismap==0) {
        if ((input.flags & BILLBOARD) > 0) {
            vpos = mul(vpos, billboard);
        } else {
            vpos = mul(float4(vpos, 1.0), input.rotation).xyz;
        }
    }

    output.flags = input.flags;

    output.position = mul(proj, mul(view, float4(input.pos + vpos, 1.0)));
    output.color = input.color;

    output.fade_dist = distance(player_pos, input.pos);
    
    if (ismap==0) {
        output.fade_alpha = distance_fade_alpha(input.fade_near, input.fade_far, output.fade_dist);
    } else {
        output.fade_alpha = 1.0;
    }

    output.cam_player_dist = distance(camera_pos, player_pos);
    output.vert_cam_dist   = distance(camera_pos, input.pos);

    return output;
}
