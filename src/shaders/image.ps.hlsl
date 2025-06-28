// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT
#include "image.hlsl"

Texture2D    texture    : register(t0);
SamplerState texsampler : register(s0);

float4 main(PSInput input) : SV_Target {
    float4 texcolor = texture.Sample(texsampler, input.texuv);

    float a = texcolor.a * color.a;

    return float4((texcolor.rgb * color.rgb) * a, a);
}
