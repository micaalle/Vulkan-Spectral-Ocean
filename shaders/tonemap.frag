#version 450

layout(location=0) in vec2 vUV;
layout(location=0) out vec4 oColor;

layout(set=0, binding=0) uniform sampler2D uHDR;

layout(push_constant) uniform PC {
    float exposure;
} pc;

void main(){
    vec3 hdr = texture(uHDR, vUV).rgb;
    hdr *= max(pc.exposure, 0.0);
    vec3 ldr = hdr / (vec3(1.0) + hdr);
    ldr = pow(max(ldr, vec3(0.0)), vec3(1.0/2.2));
    oColor = vec4(ldr, 1.0);
}
