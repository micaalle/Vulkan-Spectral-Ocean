#version 450

#define MAX_PARTICLES 16384

layout(set=0, binding=0) uniform GlobalUBO {
    mat4 view;
    mat4 proj;
    vec4 cameraPos_time;
    vec4 wave0;
    vec4 worldOrigin_pad;
    vec4 wave1;
    ivec4 debug;
    vec4 screen;
} u;

struct Particle {
    vec4 posLife; // xyz position, w life
    vec4 velSeed; // xyz velocity, w seed/size
};

layout(set=1, binding=0, std430) readonly buffer Particles {
    Particle p[];
} particles;

layout(location=0) out vec2 vQuad;
layout(location=1) out float vAlpha;

vec2 corner(int vid){
    if (vid == 0) return vec2(-1,-1);
    if (vid == 1) return vec2( 1,-1);
    if (vid == 2) return vec2(-1, 1);
    if (vid == 3) return vec2(-1, 1);
    if (vid == 4) return vec2( 1,-1);
    return vec2( 1, 1);
}

void main(){
    uint id = gl_InstanceIndex;
    Particle prt = particles.p[id];
    float life = prt.posLife.w;
    if (life <= 0.0){
        vAlpha = 0.0;
        vQuad = vec2(0.0);
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        return;
    }

    vec3 pos = prt.posLife.xyz;

    vec3 right = vec3(u.view[0][0], u.view[1][0], u.view[2][0]);
    vec3 up    = vec3(u.view[0][1], u.view[1][1], u.view[2][1]);

    vec2 c = corner(int(gl_VertexIndex));

    float size = mix(0.6, 1.8, clamp(prt.velSeed.w, 0.0, 1.0));
    vec3 wpos = pos + right * (c.x * 0.5 * size) + up * (c.y * 0.5 * size);

    gl_Position = u.proj * u.view * vec4(wpos, 1.0);
    vQuad = c * 0.5 + 0.5;

    vAlpha = clamp(life / 1.2, 0.0, 1.0);
}
