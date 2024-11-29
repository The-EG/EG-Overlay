#version 460

layout(location = 0) in vec3 fragTexCoord;

layout(location = 1) uniform vec4 color;

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2DArray texSampler;

void main() {
    ivec3 c = ivec3(floor(fragTexCoord.x), floor(fragTexCoord.y), floor(fragTexCoord.z));
    vec4 t = texelFetch(texSampler, c, 0);

    float alpha = t.r * color.a;
    outColor = vec4(color.rgb * alpha, alpha);
}
