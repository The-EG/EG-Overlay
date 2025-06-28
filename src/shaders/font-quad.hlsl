// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT
#pragma once

struct PSInput {
    float4 position : SV_Position;
    float3 texuvw   : TEXUVW;
};

cbuffer constants : register(b0) {
    float left;
    float top;
    float right;
    float bottom;

    float texleft;
    float textop;
    float texright;
    float texbottom;

    float texlayer;
    //float pad1;
    //float pad2;
    //float pad3;

    float4 color;
    float4x4 proj;
};
