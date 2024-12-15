#include "font-quad.hlsl"

PSInput main(uint vertid : SV_VertexID) {
    PSInput r;
    switch (vertid) {
    case 0:
        r.position = mul(proj, float4(right, bottom, 0.0, 1.0));
        r.texuvw   = float3(texright, texbottom, texlayer);
        break;
    case 1:
        r.position = mul(proj, float4(left, bottom, 0.0, 1.0));
        r.texuvw   = float3(texleft, texbottom, texlayer);
        break;
    case 2:
        r.position = mul(proj, float4(right, top, 0.0, 1.0));
        r.texuvw   = float3(texright, textop, texlayer);
        break;
    case 3:
        r.position = mul(proj, float4(left, top, 0.0, 1.0));
        r.texuvw   = float3(texleft, textop, texlayer);
        break;
    }

    return r;
}
