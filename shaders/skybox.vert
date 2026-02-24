#version 450

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

layout(location=0) out vec3 vDir;

vec3 cubePos(int i){
    const vec3 v[36] = vec3[36](
        vec3(-1,-1,-1), vec3( 1,-1,-1), vec3( 1, 1,-1), vec3( 1, 1,-1), vec3(-1, 1,-1), vec3(-1,-1,-1),
        vec3(-1,-1, 1), vec3( 1,-1, 1), vec3( 1, 1, 1), vec3( 1, 1, 1), vec3(-1, 1, 1), vec3(-1,-1, 1),
        vec3(-1, 1, 1), vec3(-1, 1,-1), vec3(-1,-1,-1), vec3(-1,-1,-1), vec3(-1,-1, 1), vec3(-1, 1, 1),
        vec3( 1, 1, 1), vec3( 1, 1,-1), vec3( 1,-1,-1), vec3( 1,-1,-1), vec3( 1,-1, 1), vec3( 1, 1, 1),
        vec3(-1,-1,-1), vec3( 1,-1,-1), vec3( 1,-1, 1), vec3( 1,-1, 1), vec3(-1,-1, 1), vec3(-1,-1,-1),
        vec3(-1, 1,-1), vec3( 1, 1,-1), vec3( 1, 1, 1), vec3( 1, 1, 1), vec3(-1, 1, 1), vec3(-1, 1,-1)
    );
    return v[i];
}

void main(){
    vec3 pos = cubePos(int(gl_VertexIndex));
    vDir = pos;

    mat4 viewNoTrans = u.view;
    viewNoTrans[3] = vec4(0,0,0,1);

    vec4 clip = u.proj * viewNoTrans * vec4(pos, 1.0);
    gl_Position = clip.xyww; 
}
