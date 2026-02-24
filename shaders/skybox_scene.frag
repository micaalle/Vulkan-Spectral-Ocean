#version 450

layout(location=0) in vec3 vDir;
layout(location=0) out vec4 outColor;

layout(set=1, binding=1) uniform sampler2D uEnvHDR;

#define PI 3.141592653589793

vec2 dirToEquirectUV(vec3 d){
    d = normalize(d);
    float u0 = atan(d.z, d.x);
    float v0 = asin(clamp(d.y, -1.0, 1.0));
    u0 = u0 * (1.0 / (2.0 * PI)) + 0.5;
    v0 = v0 * (1.0 / PI) + 0.5;
    return vec2(u0, v0);
}

void main(){
    float exposure = 0.95;
    vec2 uv = dirToEquirectUV(vDir);
    vec3 col = textureLod(uEnvHDR, uv, 0.0).rgb * exposure;
    outColor = vec4(col, 1.0);
}
