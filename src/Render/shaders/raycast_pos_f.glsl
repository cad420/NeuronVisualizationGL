#version 430 core
in vec3 world_pos;
uniform bool entry;
uniform bool inside;
uniform vec3 board_length;
uniform vec3 camera_pos;
out vec4 frag_color;
void main() {
    if(entry && inside){
        frag_color=vec4(camera_pos/29581,1.0);
    }
    else{
        frag_color=vec4(world_pos/29581,0.5);
    }

}
