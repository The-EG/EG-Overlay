#version 460

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec4 fragColor;
layout(location = 2) flat in uint flags;

layout(location = 0) out vec4 outColor;

layout(location = 6) uniform float vpwidth;
layout(location = 7) uniform float vpheight;

uniform sampler2D texSampler;

#define CENTERFADE (1u << 1)

void main() {
    vec4 texcolor = texture(texSampler, fragTexCoord);

    vec2 center = vec2(vpwidth / 2.0, vpheight / 2.0);
    float centerdist = abs(distance(center, gl_FragCoord.xy));

    float center_region = vpheight / 2.5;
    float center_fade = 1.0;

    if ((flags & CENTERFADE) > 0 && centerdist <= center_region) {
        center_fade = max(centerdist / center_region, 0.1);
    }

    float alpha = texcolor.a * center_fade;

    // a bit of a hack, but this keeps the transparent areas of sprites rendered
    // after others from occluding the non transparent sprites behind them
    // without this the order that sprites are rendered (declared) matters
    if (alpha < 0.05) discard;

    // TODO: color tinting
    outColor = vec4(texcolor.rgb * alpha, alpha);
}
