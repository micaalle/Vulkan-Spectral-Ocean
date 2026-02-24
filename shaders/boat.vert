#version 450

// Rasterized rubber-duck OBJ mesh that floats on the ocean.
// Vertex input: location0=pos, location1=normal, location2=uv.

layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNrm;
layout(location=2) in vec2 aUV;

layout(set=0, binding=0) uniform Global {
    mat4 view;
    mat4 proj;
    vec4 cameraPos_time;   // xyz, time
    vec4 wave0;            // patchSize, heightScale, choppy, swellAmp
    vec4 worldOrigin_pad;  // worldOrigin.xy
    vec4 wave1;            // swellSpeed, dayNight, envExposure, envMaxMip
    ivec4 debug;
    vec4 screen;           // invRes.xy, nearZ, farZ
    vec4 boat0;            // (unused)
    vec4 boat1;            // (unused)
} u;

layout(set=1, binding=0) uniform sampler2D uFFT; // packed FFT (combined)

layout(push_constant) uniform PC {
    vec4 boat0; // x,z,yawRad, scaleMeters
    vec4 boat1; // len, wid, height, draft
} pc;

layout(location=0) out vec3 vWorldPos;
layout(location=1) out vec3 vWorldNrm;
layout(location=2) out vec2 vUV;
layout(location=3) out vec3 vLocalPos; // normalized model-space (for optional procedural detail)

#define PI 3.141592653589793
const int N = 256;

// ---- simple noise (match water) ----
uint hash_u32(uint x){
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}
float hash01(ivec2 p){
    uint h = hash_u32(uint(p.x) * 1664525u + uint(p.y) * 1013904223u + 1337u);
    return float(h) / 4294967296.0;
}
float valueNoise(vec2 p){
    ivec2 i = ivec2(floor(p));
    vec2 f = fract(p);
    vec2 u0 = f*f*(3.0-2.0*f);
    float a = hash01(i + ivec2(0,0));
    float b = hash01(i + ivec2(1,0));
    float c = hash01(i + ivec2(0,1));
    float d = hash01(i + ivec2(1,1));
    return mix(mix(a,b,u0.x), mix(c,d,u0.x), u0.y);
}
vec2 macroWarp(vec2 worldXZ){
    vec2 p = worldXZ * 0.00035;
    float n1 = valueNoise(p);
    float n2 = valueNoise(p + vec2(19.7, 7.3));
    vec2 n = vec2(n1, n2) * 2.0 - 1.0;
    return n * 18.0;
}
vec2 wrap01(vec2 uv){
    vec2 f = fract(uv);
    const float eps = 1e-6;
    f = mix(f, vec2(0.0), greaterThan(f, vec2(1.0 - eps)));
    return f;
}

float texelTile(int tile, int x, int y){
    int ix = tile * N + x;
    return texelFetch(uFFT, ivec2(ix, y), 0).r;
}
float sampleReal(int tile, vec2 uv){
    vec2 w = wrap01(uv);
    float fx = w.x * float(N);
    float fy = w.y * float(N);

    int x0 = int(floor(fx)) % N;
    int y0 = int(floor(fy)) % N;

    float tx = fx - floor(fx);
    float ty = fy - floor(fy);

    int x1 = (x0 + 1) % N;
    int y1 = (y0 + 1) % N;

    float a = texelTile(tile, x0, y0);
    float b = texelTile(tile, x1, y0);
    float c = texelTile(tile, x0, y1);
    float d = texelTile(tile, x1, y1);

    float ab = mix(a, b, tx);
    float cd = mix(c, d, tx);

    return mix(ab, cd, ty);
}

mat2 rot2(float a){
    float c = cos(a), s = sin(a);
    return mat2(c, -s, s, c);
}

struct WaveSample { float h; float dx; float dz; };

// ride the cascades from water.vert
WaveSample oceanSampleCasc(vec2 worldXZ){
    float patchSize   = u.wave0.x;
    float heightScale = u.wave0.y;
    float choppy      = u.wave0.z;

    vec2 worldOrigin = u.worldOrigin_pad.xy;
    vec2 camWorldXZ  = worldOrigin + u.cameraPos_time.xz;
    float dist       = length(worldXZ - camWorldXZ);

    // same as water.vert
    float patchNear = patchSize * 1.0;
    float patchMid  = patchSize * 4.0;
    float patchFar  = patchSize * 16.0;

    float wNear = 1.0 - smoothstep(250.0, 1400.0, dist);
    float wMid  = smoothstep(450.0, 1400.0, dist) * (1.0 - smoothstep(2600.0, 9000.0, dist));
    float wFar  = 1.0 - wNear - wMid;
    wFar = clamp(wFar, 0.0, 1.0);
    wFar = max(wFar, 0.18);
    float wSum = max(1e-5, wNear + wMid + wFar);
    wNear /= wSum; wMid /= wSum; wFar /= wSum;

    vec2 warpFar  = macroWarp(worldXZ * 0.35) * 3.0;
    vec2 warpMid  = macroWarp(worldXZ * 0.70) * 1.6;
    vec2 warpNear = macroWarp(worldXZ * 1.25) * 0.8;

    vec2 uvFar  = (rot2( 0.12) * (worldXZ + warpFar))  / patchFar;
    vec2 uvMid  = (rot2( 0.35) * (worldXZ + warpMid))  / patchMid;
    vec2 uvNear = (rot2(-0.75) * (worldXZ + warpNear)) / patchNear;

    float hFar  = sampleReal(0, uvFar);
    float dxFar = sampleReal(1, uvFar);
    float dzFar = sampleReal(2, uvFar);

    float hMid  = sampleReal(0, uvMid);
    float dxMid = sampleReal(1, uvMid);
    float dzMid = sampleReal(2, uvMid);

    float hNear  = sampleReal(0, uvNear);
    float dxNear = sampleReal(1, uvNear);
    float dzNear = sampleReal(2, uvNear);

    hFar  *= 1.65; dxFar *= 1.65; dzFar *= 1.65;
    hMid  *= 1.00; dxMid *= 1.00; dzMid *= 1.00;
    hNear *= 0.55; dxNear *= 0.55; dzNear *= 0.55;

    float h  = hFar  * wFar + hMid  * wMid + hNear  * wNear;
    float dx = dxFar * wFar + dxMid * wMid + dxNear * wNear;
    float dz = dzFar * wFar + dzMid * wMid + dzNear * wNear;

    WaveSample s;
    s.h  = h  * heightScale;
    s.dx = dx * choppy;
    s.dz = dz * choppy;
    return s;
}

float oceanHeight(vec2 worldXZ){
    WaveSample s = oceanSampleCasc(worldXZ);
    float swellAmp   = u.wave0.w;
    float swellSpeed = u.wave1.x;
    float t = u.cameraPos_time.w;
    float swell = sin((worldXZ.x + worldXZ.y) * 0.015 + t * (swellSpeed * 1.0)) * swellAmp;
    return s.h + swell;
}

vec3 oceanNormal(vec2 worldXZ){
    float e = 1.5; 
    float hL = oceanHeight(worldXZ - vec2(e, 0));
    float hR = oceanHeight(worldXZ + vec2(e, 0));
    float hD = oceanHeight(worldXZ - vec2(0, e));
    float hU = oceanHeight(worldXZ + vec2(0, e));

    vec3 n = normalize(vec3(-(hR - hL), 2.0 * e, -(hU - hD)));
    return n;
}

void main(){
    vec2 duckXZ = pc.boat0.xy;
    float yawRad = pc.boat0.z;
    float scaleMeters = pc.boat0.w;
    float draft = pc.boat1.w;

    vec2 worldOrigin = u.worldOrigin_pad.xy;

    // apply same choppy displacement so it actually rides the wave
    WaveSample s = oceanSampleCasc(duckXZ);
    vec2 dispXZ = duckXZ + vec2(s.dx, s.dz);

    float swellAmp   = u.wave0.w;
    float swellSpeed = u.wave1.x;
    float t = u.cameraPos_time.w;
    float swell = sin((duckXZ.x + duckXZ.y) * 0.015 + t * (swellSpeed * 1.0)) * swellAmp;
    float y = (s.h + swell) - draft;

    vec3 up = oceanNormal(dispXZ);

    vec3 f0 = normalize(vec3(cos(yawRad), 0.0, sin(yawRad)));
    vec3 f  = normalize(f0 - up * dot(up, f0));
    vec3 r  = normalize(cross(up, f));

    vec3 origin = vec3(dispXZ.x - worldOrigin.x, y, dispXZ.y - worldOrigin.y);

    vec3 localP = aPos * scaleMeters;

    vec3 wpos = origin + r * localP.x + up * localP.y + f * localP.z;

    vec3 ln = normalize(aNrm);
    vec3 wn = normalize(r * ln.x + up * ln.y + f * ln.z);

    vWorldPos = wpos;
    vWorldNrm = wn;
    vUV = aUV;
    vLocalPos = aPos;

    gl_Position = u.proj * u.view * vec4(wpos, 1.0);
}
