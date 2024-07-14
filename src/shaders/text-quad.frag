#version 460

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

uniform sampler2DRect texSampler;

void main() {
    vec4 t = texture(texSampler, fragTexCoord);

    float alpha = t.r * fragColor.a;
    outColor = vec4(fragColor.rgb, alpha);
}