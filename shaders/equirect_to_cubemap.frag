#version 450

layout(location=0) in vec3 vDir;
layout(location=0) out vec4 outColor;

layout(set=0, binding=0) uniform sampler2D uEquirect;

const vec2 invAtan = vec2(0.15915494, 0.318309886); 

vec2 dirToEquirectUV(vec3 d){
    d = normalize(d);
    float u = atan(d.z, d.x);
    float v = asin(clamp(d.y, -1.0, 1.0));
    u = u * invAtan.x + 0.5;
    v = v * invAtan.y + 0.5;
    return vec2(u, v);
}

void main(){
    vec2 uv = dirToEquirectUV(vDir);
    vec3 hdr = texture(uEquirect, uv).rgb;
    outColor = vec4(hdr, 1.0);
}
