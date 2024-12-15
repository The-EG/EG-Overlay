#include "rect.hlsl"

float4 main(PSInput input) : SV_Target {
    float4 color = input.color;
    color.rgb *= color.a;

    return color;
}
