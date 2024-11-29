#version 460

layout(location = 0) in float left;
layout(location = 1) in float top;
layout(location = 2) in float right;
layout(location = 3) in float bottom;
layout(location = 4) in float texLeft;
layout(location = 5) in float texTop;
layout(location = 6) in float texRight;
layout(location = 7) in float texBottom;
layout(location = 8) in float texLayer;

layout(location = 0) uniform mat4 proj;

layout(location = 0) out vec3 fragTexCoord;

void main() {    
    switch (gl_VertexID) {
    // C----D
    // |    |
    // A----B
    // triangle 1 = b,a,d; triangle 2 = d,a,c
    case 0: // B
        gl_Position = proj * vec4(right, bottom, 0.0, 1.0);
        fragTexCoord = vec3(texRight, texBottom, texLayer);
        break;
    case 1: // A
        gl_Position = proj * vec4(left, bottom, 0.0, 1.0);
        fragTexCoord = vec3(texLeft, texBottom, texLayer);
        break;
    case 2: // D
        gl_Position = proj * vec4(right, top, 0.0, 1.0);
        fragTexCoord = vec3(texRight, texTop, texLayer);
        break;
    case 3: // C
        gl_Position = proj * vec4(left, top, 0.0, 1.0);
        fragTexCoord = vec3(texLeft, texTop, texLayer);
        break;
    default:
        gl_Position = proj * vec4(0.0, 0.0, 0.0, 1.0);
        fragTexCoord = vec3(0.0, 0.0, texLayer);
        break;
    }
}
