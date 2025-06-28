// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT
#include "image.hlsl"

PSInput main(uint vertid : SV_VertexID) {
    PSInput r;

    switch (vertid) {
    case 0:
        r.position = mul(proj, float4(right, bottom, 0.0, 1.0));
        r.texuv    = float2(maxu, maxv);
        break;
    case 1:
        r.position = mul(proj, float4(left, bottom, 0.0, 1.0));
        r.texuv    = float2(0.0, maxv);
        break;
    case 2:
        r.position = mul(proj, float4(right, top, 0.0, 1.0));
        r.texuv    = float2(maxu, 0.0);
        break;
    case 3:
        r.position = mul(proj, float4(left, top, 0.0, 1.0));
        r.texuv    = float2(0.0, 0.0);
        break;
    }

    return r;
}
