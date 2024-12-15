#include "rect.hlsl"

cbuffer constants : register(b0) {
    float    left;
    float    top;
    float    right;
    float    bottom;
    float4   color;
    float4x4 proj;
};

PSInput main(uint vertid : SV_VertexID) {
    PSInput r;

    switch (vertid) {
    case 0: 
        r.position = mul(proj, float4(left, top, 0.0, 1.0));
        break;
    case 1:
        r.position = mul(proj, float4(right, top, 0.0, 1.0));
        break;
    case 2: 
        r.position = mul(proj, float4(left, bottom, 0.0, 1.0));
        break;
    case 3:
        r.position = mul(proj, float4(right, bottom, 0.0, 1.0));
        break;
    }

    r.color = color;

    return r;
}
