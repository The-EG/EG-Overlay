#pragma once

struct PSInput {
    float4 position : SV_Position;
    float2 texuv    : TEXUV;
};

// root constants
//  0  1 float    left
//  1  1 float    top
//  2  1 float    right
//  3  1 float    bottom
//  4  4 float4   color
//  8 16 float4x4 proj
// 24  1 float    maxu
// 25  1 float    maxv
cbuffer constants : register(b0) {
    float left;
    float top;
    float right;
    float bottom;

   
    float4 color;

    float4x4 proj;

    float maxu;
    float maxv;
};
