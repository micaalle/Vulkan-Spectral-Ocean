#version 450

layout(push_constant) uniform PC {
    mat4 view;
    mat4 proj;
} pc;

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
    gl_Position = pc.proj * pc.view * vec4(pos, 1.0);
}
