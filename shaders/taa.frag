#version 450

layout(location=0) in vec2 vUV;
layout(location=0) out vec4 oColor;

// set=0 bindings
layout(set=0, binding=0) uniform TaaUBO {
    mat4 invCurrVP;
    mat4 prevVP;
    vec4 params; 
} u;

layout(set=0, binding=1) uniform sampler2D uCurr;
layout(set=0, binding=2) uniform sampler2D uDepth;
layout(set=0, binding=3) uniform sampler2D uHist;

vec3 neighborhoodMin(vec2 uv, vec2 texel){
    vec3 c  = texture(uCurr, uv).rgb;
    vec3 c1 = texture(uCurr, uv + vec2(texel.x, 0)).rgb;
    vec3 c2 = texture(uCurr, uv + vec2(-texel.x,0)).rgb;
    vec3 c3 = texture(uCurr, uv + vec2(0, texel.y)).rgb;
    vec3 c4 = texture(uCurr, uv + vec2(0,-texel.y)).rgb;
    return min(c, min(min(c1,c2), min(c3,c4)));
}

vec3 neighborhoodMax(vec2 uv, vec2 texel){
    vec3 c  = texture(uCurr, uv).rgb;
    vec3 c1 = texture(uCurr, uv + vec2(texel.x, 0)).rgb;
    vec3 c2 = texture(uCurr, uv + vec2(-texel.x,0)).rgb;
    vec3 c3 = texture(uCurr, uv + vec2(0, texel.y)).rgb;
    vec3 c4 = texture(uCurr, uv + vec2(0,-texel.y)).rgb;
    return max(c, max(max(c1,c2), max(c3,c4)));
}

void main(){
    vec2 invRes = u.params.xy;
    float alpha = clamp(u.params.z, 0.0, 1.0);
    vec2 texel = invRes;

    // cur color
    vec3 curr = texture(uCurr, vUV).rgb;

    // depth 
    float depth = texture(uDepth, vUV).r;

    vec2 ndcXY = vUV * 2.0 - 1.0;
    vec4 ndc = vec4(ndcXY, depth, 1.0);
    vec4 world = u.invCurrVP * ndc;
    world.xyz /= max(world.w, 1e-6);

    vec4 prevClip = u.prevVP * vec4(world.xyz, 1.0);
    vec3 prevNdc = prevClip.xyz / max(prevClip.w, 1e-6);
    vec2 prevUV = prevNdc.xy * 0.5 + 0.5;

    // if it goes offscreen dont apply to history
    if (any(lessThan(prevUV, vec2(0.0))) || any(greaterThan(prevUV, vec2(1.0)))){
        oColor = vec4(curr, 1.0);
        return;
    }

    vec3 hist = texture(uHist, prevUV).rgb;

    // reduce the ghosting
    vec3 mn = neighborhoodMin(vUV, texel);
    vec3 mx = neighborhoodMax(vUV, texel);
    hist = clamp(hist, mn, mx);

    vec3 outc = mix(curr, hist, alpha);
    oColor = vec4(outc, 1.0);
}
