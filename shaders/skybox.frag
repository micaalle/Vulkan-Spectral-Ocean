#version 450

layout(location=0) in vec3 vDir;
layout(location=0) out vec4 outColor;

layout(set=0, binding=0) uniform Global {
    mat4 view;
    mat4 proj;
    vec4 cameraPos_time;
    vec4 wave0;
    vec4 worldOrigin_pad;
    vec4 wave1;
    ivec4 debug;
    vec4 screen;
} u;

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

vec3 tonemap(vec3 c){
    c = c / (vec3(1.0) + c);
    c = pow(c, vec3(1.0/2.2));
    return c;
}

void main(){
    float exposure = 0.95;
    vec2 uv = dirToEquirectUV(vDir);
    vec3 col = textureLod(uEnvHDR, uv, 0.0).rgb * exposure;
    outColor = vec4(tonemap(col), 1.0);
}
