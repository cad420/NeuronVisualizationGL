#version 460 core

layout(location=0) in vec4 vertexPos;
flat out uint box_idx;
uniform mat4 MVPMatrix;
void main() {
    gl_Position=MVPMatrix*vec4(vertexPos.xyz,1.f);
    box_idx=uint(vertexPos.w);
}
