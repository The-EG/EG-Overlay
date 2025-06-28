// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT
#pragma once
// b0: root constants:
//         0 -  1 - float    left
//         1 -  1 - float    top
//         2 -  1 - float    right
//         3 -  1 - float    bottom
//         4 -  4 - float4   color
//         8 - 16 - float4x4 proj

struct PSInput {
    float4 position : SV_Position;
    float4 color    : COLOR;
};
