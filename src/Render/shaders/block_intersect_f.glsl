#version 460 core
out vec4 frag_color;
flat in uint box_idx;
layout(std430,binding=0) buffer IntersectBox{
    uint box[];
}intersectBox;

void main() {

    intersectBox.box[box_idx]=1;
    frag_color=vec4(1.f,0.f,0.f,0.f);
}
