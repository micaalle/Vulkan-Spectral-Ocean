#version 450

layout(location=0) in vec3 vWorldPos;
layout(location=1) in vec3 vWorldNrm;
layout(location=2) in vec2 vUV;
layout(location=3) in vec3 vLocalPos;

layout(location=0) out vec4 outColor;

layout(set=0, binding=0) uniform Global {
    mat4 view;
    mat4 proj;
    vec4 cameraPos_time;   // xyz, time
    vec4 wave0;
    vec4 worldOrigin_pad;
    vec4 wave1;            // swellSpeed, dayNight, envExposure, envMaxMip
    ivec4 debug;
    vec4 screen;           // invRes.xy, nearZ, farZ
    vec4 boat0;
    vec4 boat1;
} u;

layout(set=1, binding=1) uniform sampler2D uEnvHDR;

const vec2 invAtan = vec2(0.15915494, 0.318309886);

vec2 dirToEquirectUV(vec3 d){
    d = normalize(d);
    float u0 = atan(d.z, d.x);
    float v0 = asin(clamp(d.y, -1.0, 1.0));
    u0 = u0 * invAtan.x + 0.5;
    v0 = v0 * invAtan.y + 0.5;
    return vec2(u0, v0);
}

vec3 sampleEnv(vec3 dir){
    vec2 uv = dirToEquirectUV(dir);
    return textureLod(uEnvHDR, uv, 0.0).rgb;
}

void main(){
    vec3 N = normalize(vWorldNrm);
    vec3 V = normalize(u.cameraPos_time.xyz - vWorldPos);

    vec3 L = normalize(vec3(0.35, 1.0, 0.15));
    float ndl = clamp(dot(N, L), 0.0, 1.0);

    float ramp = smoothstep(0.05, 0.20, ndl) * 0.55 + smoothstep(0.25, 0.80, ndl) * 0.45;

    // make him yellow
    vec3 base = vec3(1.00, 0.86, 0.12);
    float under = clamp((N.y * 0.5 + 0.5), 0.0, 1.0);
    base *= mix(0.75, 1.05, under);

    // highlights 
    vec3 H = normalize(L + V);
    float spec = pow(clamp(dot(N, H), 0.0, 1.0), 64.0) * 0.35;

    // rubbery refect 
    vec3 R = reflect(-V, N);
    vec3 env = sampleEnv(R) * (0.06 + 0.04 * (1.0 - ndl));
    float rim = pow(clamp(1.0 - dot(N, V), 0.0, 1.0), 3.0) * 0.35;

    float beak = smoothstep(0.15, 0.28, vLocalPos.z) * smoothstep(0.10, -0.02, abs(vLocalPos.x)) * smoothstep(0.15, 0.05, abs(vLocalPos.y - 0.05));
    vec3 beakCol = vec3(1.0, 0.45, 0.06);

    vec3 col = base * (0.25 + 0.95 * ramp) + env + spec + rim;
    col = mix(col, beakCol, beak * 0.85);

    col *= u.wave1.z;

    outColor = vec4(col, 1.0);
}
