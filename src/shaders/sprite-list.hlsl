#pragma once

// constants
//  0  16  float4x4  view
// 16  16  float4x4  proj
// 32   3  float3    player_pos
// 35   1  uint      ismap
// 36   3  float3    camera_pos
// 39   1  float     map_left
// 40   1  float     map_bottom
// 41   1  float     map_height

cbuffer constants : register(b0) {
    float4x4 view;

    float4x4 proj;

    float3   player_pos;
    uint     ismap;

    float3   camera_pos;
    float    map_left;

    float    map_top;
    float    map_height;
};

struct PSInput {
    float4 position        : SV_Position;
    float2 texuv           : TEXUV;
    float4 color           : COLOR;
    uint   flags           : FLAGS;
    float  fade_alpha      : FADE_ALPHA;
    float  fade_dist       : FADE_DIST;
    float  cam_player_dist : CAM_PLAYER_DIST;
    float  vert_cam_dist   : VERT_CAM_DIST;
};
