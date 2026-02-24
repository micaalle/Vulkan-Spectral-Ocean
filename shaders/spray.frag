#version 450

layout(location=0) in vec2 vQuad;
layout(location=1) in float vAlpha;
layout(location=0) out vec4 oColor;

void main(){
    vec2 p = vQuad * 2.0 - 1.0;
    float r2 = dot(p,p);
    float a = smoothstep(1.0, 0.0, r2);
    a *= vAlpha;
    if (a < 0.01) discard;

    vec3 col = vec3(1.0);
    col *= 1.15;
    oColor = vec4(col * a, a);
}
