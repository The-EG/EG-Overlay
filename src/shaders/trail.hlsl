#pragma once
// root constants
//  0 16 float4x4 view
// 16 16 float4x4 proj
// 32  4 float4   color
// 36  3 float3   player_pos
// 39  1 uint     inmap
// 40  3 float3   camera_pos
// 43  1 float    fade_near
// 44  1 float    fade_far
// 45  1 float    map_left
// 46  1 float    map_top
// 47  1 float    map_height

struct PSInput {
    float4 position        : SV_Position;
    float2 texuv           : TEXUV;
    float  fade_dist       : FADE_DIST;
    float3 trail_pos       : TRAIL_POS;
    float  cam_player_dist : CAM_PLAYER_DIST;
    float  vert_cam_dist   : VERT_CAM_DIST;
};

cbuffer constants : register(b0) {
    float4x4 view;
    float4x4 proj;
    float4   color;
    float3   player_pos;
    uint     inmap;
    float3   camera_pos;
    float    fade_near;
    float    fade_far;
    float    map_left;
    float    map_top;
    float    map_height;
};
