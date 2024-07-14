#version 460

layout(location = 7) uniform float value_factor;
layout(location = 8) uniform float saturation_factor;

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

uniform sampler2D texSampler;

//https://stackoverflow.com/a/17897228
// All components are in the range [0…1], including hue.
vec3 rgb2hsv(vec3 c)
{
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));

    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

// All components are in the range [0…1], including hue.
vec3 hsv2rgb(vec3 c)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}


void main() {
    vec4 texcolor = texture(texSampler, fragTexCoord);
    if (value_factor!=1.0 || saturation_factor!=1.0) {
        vec3 hsv = rgb2hsv(texcolor.rgb);
        hsv.b = clamp(hsv.b * value_factor, 0.0, 1.0);
        hsv.g = clamp(hsv.g * saturation_factor, 0.0, 1.0);
        vec3 rgb = hsv2rgb(hsv);

        outColor = vec4(rgb, texcolor.a);
    } else {
        outColor = texcolor;
    }
}