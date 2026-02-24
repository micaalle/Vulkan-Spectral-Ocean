#version 450

layout(location=0) in vec3 vPos;
layout(location=1) in vec2 vUV;
layout(location=2) in vec2 vWorldXZ;
layout(location=3) in vec2 vUVFFT;

layout(location=0) out vec4 outColor;

layout(set=0, binding=0) uniform Global {
    mat4 view;
    mat4 proj;
    vec4 cameraPos_time;   // xyz, time
    vec4 wave0;            // patchSize, heightScale, choppy, swellAmp
    vec4 worldOrigin_pad;  // worldOrigin.xy
    vec4 wave1;            // swellSpeed, dayNight, envExposure, envMaxMip
    ivec4 debug;
    vec4 screen;           // invRes.xy, nearZ, farZ
    vec4 boat0;            // boatPos.x, boatPos.z, boatYaw(rad), wakePatch
    vec4 boat1;            // boatSpeed, boatLen, boatWid, draft
} u;

layout(set=1, binding=0) uniform sampler2D uFFT;       
layout(set=1, binding=1) uniform sampler2D uEnvHDR;
layout(set=1, binding=2) uniform sampler2D uFoam;
layout(set=1, binding=3) uniform sampler2D uSceneColor;
layout(set=1, binding=4) uniform sampler2D uSceneDepth;
layout(set=1, binding=5) uniform sampler2D uFFTDetail;  
layout(set=1, binding=6) uniform sampler2D uWake;       

#define PI 3.141592653589793

const int N = 256;

mat2 rot2(float a);
vec2 macroWarp(vec2 worldXZ, float freq, float amp);

vec2 wrap01(vec2 uv){
    vec2 f = fract(uv);
    const float eps = 1e-6;
    f = mix(f, vec2(0.0), greaterThan(f, vec2(1.0 - eps)));
    return f;
}

float texelTile(sampler2D tex, int tile, int x, int y){
    int ix = tile * N + x;
    return texelFetch(tex, ivec2(ix, y), 0).r;
}

float sampleReal(sampler2D tex, int tile, vec2 uv){
    vec2 w = wrap01(uv);
    float fx = w.x * float(N);
    float fy = w.y * float(N);

    int x0 = int(floor(fx)) % N;
    int y0 = int(floor(fy)) % N;

    float tx = fx - floor(fx);
    float ty = fy - floor(fy);

    int x1 = (x0 + 1) % N;
    int y1 = (y0 + 1) % N;

    float a = texelTile(tex, tile, x0, y0);
    float b = texelTile(tex, tile, x1, y0);
    float c = texelTile(tex, tile, x0, y1);
    float d = texelTile(tex, tile, x1, y1);

    float ab = mix(a, b, tx);
    float cd = mix(c, d, tx);
    return mix(ab, cd, ty);
}

float sampleH_comb(vec2 uv){ return sampleReal(uFFT, 0, uv); }
float sampleH_wind(vec2 uv){ return sampleReal(uFFTDetail, 0, uv); }

// sample the same FFT at multiple world patch sizes and blend by distance 
struct CascadeW { float n; float m; float f; };

CascadeW cascadeWeights(float dist){
    CascadeW w;
    w.n = 1.0 - smoothstep(250.0, 1400.0, dist);
    w.m = smoothstep(450.0, 1400.0, dist) * (1.0 - smoothstep(2600.0, 9000.0, dist));
    w.f = 1.0 - w.n - w.m;
    w.f = clamp(w.f, 0.0, 1.0);
    w.f = max(w.f, 0.18);
    float s = max(1e-5, w.n + w.m + w.f);
    w.n /= s; w.m /= s; w.f /= s;
    return w;
}

void cascadeUVs(vec2 worldXZ, float basePatch, out vec2 uvFar, out vec2 uvMid, out vec2 uvNear){
    float patchNear = basePatch * 1.0;
    float patchMid  = basePatch * 4.0;
    float patchFar  = basePatch * 16.0;

    // cascade warp 
    vec2 warpFar  = macroWarp(worldXZ * 0.35, 0.00035, 18.0) * 3.0;
    vec2 warpMid  = macroWarp(worldXZ * 0.70, 0.00035, 18.0) * 1.6;
    vec2 warpNear = macroWarp(worldXZ * 1.25, 0.00035, 18.0) * 0.8;

    uvFar  = (rot2( 0.12) * (worldXZ + warpFar))  / patchFar;
    uvMid  = (rot2( 0.35) * (worldXZ + warpMid))  / patchMid;
    uvNear = (rot2(-0.75) * (worldXZ + warpNear)) / patchNear;
}

float sampleH_cascade_comb(vec2 worldXZ, float dist, float basePatch){
    CascadeW w = cascadeWeights(dist);
    vec2 uvF, uvM, uvN;
    cascadeUVs(worldXZ, basePatch, uvF, uvM, uvN);
    float hF = sampleH_comb(uvF) * 1.65;
    float hM = sampleH_comb(uvM) * 1.00;
    float hN = sampleH_comb(uvN) * 0.55;
    return hF*w.f + hM*w.m + hN*w.n;
}

 // wind shouldn't contribute far
float sampleH_cascade_wind(vec2 worldXZ, float dist, float basePatch){
    CascadeW w = cascadeWeights(dist);
    vec2 uvF, uvM, uvN;
    cascadeUVs(worldXZ, basePatch, uvF, uvM, uvN);
    float wN = w.n;
    float wM = w.m;
    float s = max(1e-5, wN + wM);
    wN/=s; wM/=s;
    float hM = sampleH_wind(uvM) * 0.90;
    float hN = sampleH_wind(uvN) * 1.10;
    return hM*wM + hN*wN;
}

mat2 rot2(float a){
    float c = cos(a), s = sin(a);
    return mat2(c, -s, s, c);
}


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
    vec2 u = f*f*(3.0-2.0*f);
    float a = hash01(i + ivec2(0,0));
    float b = hash01(i + ivec2(1,0));
    float c = hash01(i + ivec2(0,1));
    float d = hash01(i + ivec2(1,1));
    return mix(mix(a,b,u.x), mix(c,d,u.x), u.y);
}
vec2 macroWarp(vec2 worldXZ, float freq, float amp){
    vec2 p = worldXZ * freq;
    float n1 = valueNoise(p);
    float n2 = valueNoise(p + vec2(19.7, 7.3));
    vec2 n = vec2(n1, n2) * 2.0 - 1.0;
    return n * amp; 
}
vec2 dirToEquirectUV(vec3 d){
    d = normalize(d);
    float u0 = atan(d.z, d.x);
    float v0 = asin(clamp(d.y, -1.0, 1.0));
    u0 = u0 * (1.0 / (2.0 * PI)) + 0.5;
    v0 = v0 * (1.0 / PI) + 0.5;
    return vec2(u0, v0);
}

float linearizeDepthZO(float depth01, float nearZ, float farZ){
    return (nearZ * farZ) / max(1e-6, (farZ - depth01 * (farZ - nearZ)));
}


//  build ripples as a sum of directional sines and return dh/dx, dh/dz in world units.
vec2 rippleGrad(vec2 worldXZ, float t){
    vec2 p = worldXZ * 0.025 + vec2(t * 0.20, t * 0.17);

    vec2 d1 = normalize(vec2(1.0, 0.25));
    vec2 d2 = normalize(vec2(-0.35, 1.0));
    vec2 d3 = normalize(vec2(0.70, -0.70));

    float k1 = 26.0;
    float k2 = 33.0;
    float k3 = 41.0;

    float a1 = 0.0030;
    float a2 = 0.0022;
    float a3 = 0.0018;

    float p1 = dot(p, d1) * k1;
    float p2 = dot(p, d2) * k2;
    float p3 = dot(p, d3) * k3;

    vec2 g = vec2(0.0);
    g += a1 * cos(p1) * k1 * d1;
    g += a2 * cos(p2) * k2 * d2;
    g += a3 * cos(p3) * k3 * d3;
    return g;
}

void main(){
    float patchSize   = u.wave0.x;
    float heightScale = u.wave0.y;
    float swellAmp    = u.wave0.w;
    float dayNight    = u.wave1.y;
    float exposure    = u.wave1.z;
    float envMaxMip   = u.wave1.w;
    vec2 invRes       = u.screen.xy;
    float nearZ       = u.screen.z;
    float farZ        = u.screen.w;

    int dbg = u.debug.x;

    vec3 camPos = u.cameraPos_time.xyz;
    vec2 camWorldXZ = u.worldOrigin_pad.xy + u.cameraPos_time.xz;
    float dist = length(vWorldXZ - camWorldXZ);

    // hide LOD rings
    float windFade   = 1.0 - smoothstep(900.0, 3200.0, dist);
    float rippleFade = 1.0 - smoothstep(250.0, 1400.0, dist);

    // --- Base normal from cascaded displacement (true cascades) ---
    float stepW = patchSize / float(N);

    // Sample cascaded height in world space (avoids UV-wrap discontinuities)
    float hL = sampleH_cascade_comb(vWorldXZ - vec2(stepW, 0.0), dist, patchSize) * heightScale;
    float hR = sampleH_cascade_comb(vWorldXZ + vec2(stepW, 0.0), dist, patchSize) * heightScale;
    float hD = sampleH_cascade_comb(vWorldXZ - vec2(0.0, stepW), dist, patchSize) * heightScale;
    float hU = sampleH_cascade_comb(vWorldXZ + vec2(0.0, stepW), dist, patchSize) * heightScale;

    // Same swell phase as water.vert
    float phase = 0.015 * (vWorldXZ.x + vWorldXZ.y) + u.cameraPos_time.w * u.wave1.x;
    float swell = swellAmp * sin(phase);
    float ds = swellAmp * 0.015 * cos(phase);

    float dhdx = (hR - hL) / (2.0 * stepW) + ds;
    float dhdz = (hU - hD) / (2.0 * stepW) + ds;

    // --- Wind-band normals (cascade-aware, stronger than displacement) ---
    const float WIND_NORM_SCALE = 0.90;

    float wL = sampleH_cascade_wind(vWorldXZ - vec2(stepW, 0.0), dist, patchSize) * heightScale;
    float wR = sampleH_cascade_wind(vWorldXZ + vec2(stepW, 0.0), dist, patchSize) * heightScale;
    float wD = sampleH_cascade_wind(vWorldXZ - vec2(0.0, stepW), dist, patchSize) * heightScale;
    float wU = sampleH_cascade_wind(vWorldXZ + vec2(0.0, stepW), dist, patchSize) * heightScale;

    float dhdxW = (wR - wL) / (2.0 * stepW);
    float dhdzW = (wU - wD) / (2.0 * stepW);

    dhdx += dhdxW * (WIND_NORM_SCALE * windFade);
    dhdz += dhdzW * (WIND_NORM_SCALE * windFade);

    // --- Ripples (normal-only) ---
    vec2 gRip = rippleGrad(vWorldXZ, u.cameraPos_time.w);
    dhdx += gRip.x * rippleFade;
    dhdz += gRip.y * rippleFade;

    vec3 n = normalize(vec3(-dhdx, 1.0, -dhdz));

    if (dbg == 1){
        float h = sampleH_cascade_comb(vWorldXZ, dist, patchSize) * heightScale;
        outColor = vec4(vec3(0.5 + 0.02*h), 1.0);
        return;
    }
    if (dbg == 2){
        outColor = vec4(n*0.5 + 0.5, 1.0);
        return;
    }

    vec3 v = normalize(camPos - vPos);
    float NdotV = clamp(dot(n, v), 0.0, 1.0);

    // --- Reflection ---
    float F0 = 0.02;
    float fres = F0 + (1.0 - F0) * pow(1.0 - NdotV, 5.0);

    // Foam (temporal sim texture)
    // Foam uses stable base UV (not the warped FFT UV), so it doesn't look screen-locked.
    float foam = texture(uFoam, wrap01(vUV)).r;
    // Boat wake (local patch around boat). Stored in a separate R16F field for long trails.
    vec2 boatXZ = u.boat0.xy;
    float wakePatch = u.boat0.w;
    float wake = 0.0;
    if (wakePatch > 1.0){
        vec2 uvW = (vWorldXZ - boatXZ) / wakePatch + 0.5;
        // clamp sampling, but mask outside the patch to avoid edge smear
        float inside = step(0.0, uvW.x) * step(uvW.x, 1.0) * step(0.0, uvW.y) * step(uvW.y, 1.0);
        wake = texture(uWake, clamp(uvW, vec2(0.0), vec2(1.0))).r * inside;
    }

    foam = smoothstep(0.15, 0.75, foam);
    // Wake acts like persistent foam sheets
    wake = smoothstep(0.10, 0.85, wake);
    float foamAll = 1.0 - (1.0 - foam) * (1.0 - wake);
    foam = foamAll;

    // Roughness from slope + foam
    float slope = clamp(length(vec2(dhdx, dhdz)) * 0.35, 0.0, 1.0);
    float rough = clamp(0.035 + 0.30 * slope, 0.035, 0.55);
    rough = mix(rough, 0.90, foam);
    float lod = rough * envMaxMip;

    vec3 r = reflect(-v, n);
    vec2 envUV = dirToEquirectUV(r);
    vec3 env = textureLod(uEnvHDR, envUV, lod).rgb;

    // --- Base water color (match your OpenGL palette) ---
    vec3 deepDay      = vec3(0.006, 0.050, 0.140);
    vec3 shallowDay   = vec3(0.040, 0.390, 0.610);
    vec3 deepNight    = vec3(0.004, 0.018, 0.060);
    vec3 shallowNight = vec3(0.014, 0.085, 0.160);

    vec3 deepCol    = mix(deepNight,    deepDay,    dayNight);
    vec3 shallowCol = mix(shallowNight, shallowDay, dayNight);

    float hNow = sampleH_cascade_comb(vWorldXZ, dist, patchSize) * heightScale + swell;

    float crest = clamp(1.0 - exp(-abs(hNow) * 0.02), 0.0, 1.0);
    float grazing = pow(1.0 - NdotV, 2.0);
    float shallowT = clamp(0.10 + 0.65 * crest + 0.18 * grazing, 0.0, 1.0);

    vec3 water = mix(deepCol, shallowCol, shallowT);

    // --- Ambient + subsurface (keeps non-glinty water alive) ---
    vec3 hemiDir = normalize(n + vec3(0.0, 0.6, 0.0));
    vec3 skyHemi = textureLod(uEnvHDR, dirToEquirectUV(hemiDir), envMaxMip * 0.85).rgb;
    float skyLum = dot(skyHemi, vec3(0.2126, 0.7152, 0.0722));
    vec3 skyNeutral = mix(vec3(skyLum), skyHemi, 0.35);
    vec3 ambient = skyNeutral * exposure * (0.03 + 0.08 * (1.0 - NdotV));
    ambient *= mix(vec3(1.0), water, 0.35);
    vec3 subsurface = water * (0.08 + 0.22 * grazing);

    // --- Refraction + depth tint ---
    vec2 screenUV = gl_FragCoord.xy * invRes;

    float sceneD0 = texture(uSceneDepth, screenUV).r;
    bool hasScene0 = (sceneD0 < 0.9990);

    vec2 refrUV = screenUV;
    vec3 sceneCol;
    float sceneD;
    if (hasScene0){
        float refrStrength = mix(0.010, 0.050, pow(1.0 - NdotV, 1.5));
        refrUV = clamp(screenUV + n.xz * refrStrength, vec2(0.001), vec2(0.999));
        sceneCol = texture(uSceneColor, refrUV).rgb;
        sceneD   = texture(uSceneDepth,  refrUV).r;
    } else {
        vec3 skyBlur = textureLod(uEnvHDR, dirToEquirectUV(hemiDir), envMaxMip * 0.90).rgb;
        float skyB = dot(skyBlur, vec3(0.2126, 0.7152, 0.0722));
        sceneCol = water + vec3(skyB) * 0.02;
        sceneD   = sceneD0;
    }

    bool hasScene = (sceneD < 0.9990);

    float linScene = linearizeDepthZO(sceneD, nearZ, farZ);
    float linWater = linearizeDepthZO(gl_FragCoord.z, nearZ, farZ);

    float thickness = hasScene
        ? clamp(max(0.0, linScene - linWater), 0.0, 80.0)
        : mix(22.0, 8.0, shallowT);
    thickness = clamp(thickness, 0.0, 40.0);

    vec3 sigmaA = vec3(0.060, 0.020, 0.010);
    sigmaA *= mix(1.15, 0.90, dayNight);
    vec3 trans = exp(-sigmaA * thickness);

    float shallowByDepth = clamp(1.0 - thickness / 18.0, 0.0, 1.0);
    shallowT = clamp(max(shallowT, shallowByDepth), 0.0, 1.0);
    water = mix(deepCol, shallowCol, shallowT);

    vec3 refr = sceneCol * trans + water * (1.0 - trans);

    float hazeAmt = 1.0 - exp(-thickness * 0.06);
    hazeAmt *= hasScene ? 1.0 : 0.35;
    refr += water * hazeAmt * 0.16;

    vec3 base = refr + ambient * 0.65 + subsurface;

    // Foam is bright and diffuse, kills reflection
    vec3 foamCol = vec3(0.92);
    vec3 foamLit = foamCol * (0.25 + 0.75 * clamp(skyLum * exposure, 0.0, 2.0));
    base = mix(base, foamLit, foam * 0.90);

    vec3 refl = env * exposure * mix(1.0, 0.35, foam);
    fres = mix(fres, 0.04, foam);

    vec3 col = base * (1.0 - fres) + refl * fres;

    // --- Sea of Thieves-ish stylization ---
    // Soft rim + distance fog helps sell scale and keeps the far field from looking repetitive.
    float rim = pow(1.0 - NdotV, 3.5);
    vec3 rimCol = mix(vec3(0.03, 0.16, 0.24), vec3(0.05, 0.28, 0.42), dayNight);
    col += rimCol * rim * (0.06 + 0.10 * windFade);

    float fog = smoothstep(2200.0, 6500.0, dist);
    vec3 fogCol = skyNeutral * exposure * 0.55;
    col = mix(col, fogCol, fog * 0.35);

    outColor = vec4(col, 1.0);
}