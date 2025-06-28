// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT
#include "rect.hlsl"

float4 main(PSInput input) : SV_Target {
    float4 color = input.color;
    color.rgb *= color.a;

    return color;
}
