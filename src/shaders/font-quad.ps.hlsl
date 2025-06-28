// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT
#include "font-quad.hlsl"

Texture2DArray texture : register(t0);

float4 main(PSInput input) : SV_Target {
    int3 c = floor(input.texuvw);

    float4 t = texture[c];

    float alpha = t.r * color.a;
    return float4(color.rgb * alpha, alpha);
}
