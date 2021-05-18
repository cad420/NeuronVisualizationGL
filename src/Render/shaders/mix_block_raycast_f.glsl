#version 430 core
out vec4 frag_color;

layout(location = 0) uniform sampler1D transfer_func;
layout(location = 1) uniform sampler2D preInt_transfer_func;

layout(location = 2) uniform sampler3D cache_volume0;
layout(location = 3) uniform sampler3D cache_volume1;
layout(location = 4) uniform sampler3D cache_volume2;
layout(location = 5) uniform sampler3D cache_volume3;
layout(location = 6) uniform sampler3D cache_volume4;


layout(binding=0, rgba32f) uniform image2D entry_pos;
layout(binding=1, rgba32f) uniform image2D exit_pos;

uniform float ka;
uniform float kd;
uniform float shininess;
uniform float ks;
uniform vec4 bg_color;

uniform int block_length;
uniform int padding;
uniform ivec3 block_dim;
uniform ivec3 texture_size3;//size3 for cache_volume
uniform vec3 board_length;


uniform uint lod_mapping_table_offset[10];
uniform uint min_lod;
uniform uint max_lod;
layout(std430,binding=0) buffer MappintTable{
    uvec4 page_entry[];
}mapping_table;

uniform uint window_width;
uniform uint window_height;
uniform vec3 camera_pos;
uniform vec3 view_pos;
uniform vec3 view_direction;
uniform vec3 view_right;
uniform vec3 view_up;
uniform float view_depth;
uniform float view_right_space;
uniform float view_up_space;
uniform float step;

int virtualSample(int lod_t,vec3 samplePos,out vec4 scalar);

vec3 phongShading(int lod_t,vec3 samplePos,vec3 diffuseColor);

int sampleDefaultRaw(int lod_t,vec3 samplePos,out vec4 scalar);

uint cur_lod;

void main()
{
    vec3 start_pos=imageLoad(entry_pos,ivec2(gl_FragCoord.xy)).xyz*29581;
    vec3 end_pos=imageLoad(exit_pos,ivec2(gl_FragCoord.xy)).xyz*29581;

    vec3 ray_start_pos=start_pos;
    vec3 ray_direction=normalize(end_pos-start_pos);
    int steps=int(dot(end_pos-start_pos,ray_direction));


    vec4 color=vec4(0.f);
    vec4 last_sample_scalar=vec4(0.f);
    vec4 sample_scalar;
    vec4 sample_color;
    vec3 sample_pos=ray_start_pos;
    vec3 lod_sample_pos=sample_pos;
    int lod_steps=0;
    cur_lod=min_lod;
    int lod_t=int(pow(2,cur_lod));
    float lod_step=lod_t*step;
    for(int i=0;i<2000;i++){
        int return_flag=virtualSample(lod_t,sample_pos,sample_scalar);


        if(return_flag==-1){
//            color=vec4(1.f,0.f,0.f,1.f);
//            break;
        }
        else if(return_flag==-2){
            color=vec4(1.f,1.f,0.f,1.f);
            break;
        }
        else if(return_flag==-3){
            color=vec4(1.f,1.f,1.f,1.f);
            break;
        }
        else if(return_flag==0){
//            if(lod_t==8){
//                color=vec4(0.f,1.f,0.f,1.f);
//                break;
//            }
            cur_lod+=1;
            if(cur_lod>max_lod){
//                color=vec4(0.f,1.f,0.f,1.f);
                break;
            }
            lod_t*=2;
            lod_step=lod_t*step;
            lod_steps=i;
            lod_sample_pos=sample_pos;
            i--;
            continue;

        }
        else if(return_flag==1){
//            if(cur_lod==3){
//                color=vec4(0.f,1.f,1.f,1.f);
//                break;
//            }
//            color=vec4(0.f,0.f,1.f,1.f);
//            if(sample_scalar.r>0.3f)
//                color=vec4(sample_scalar.rgb,1.f);
//            else
//                color=vec4(1.f);
//            break;

        }
        else if(return_flag==2){
//            discard;

            color=vec4(0.f,0.f,1.f,1.f);
            break;
        }


        if(sample_scalar.r>0.3f){

            sample_color=texture(preInt_transfer_func,vec2(last_sample_scalar.r,sample_scalar.r));
            sample_color.rgb=phongShading(lod_t,sample_pos,sample_color.rgb);
//            sample_color.a*=(lod_step*lod_step-i*i)*1.f/(lod_step*lod_step);
            color=color+sample_color*vec4(sample_color.aaa,1.f)*(1.f-color.a);

            if(color.a>0.9f)
                break;
            last_sample_scalar=sample_scalar;

        }
        sample_pos=lod_sample_pos+(i-lod_steps)*ray_direction*lod_step;
    }
    if(color.a==0.f) discard;
    color+=bg_color*(1.f-color.a);

    frag_color=color;
}

// samplePos is raw world pos, measure in dim(raw_x,raw_y,raw_z)
int virtualSample(int lod_t,vec3 samplePos,out vec4 scalar)
{
    //1. first get virtual_index in block-dim
    int lod_no_padding_block_length=(block_length-2*padding)*lod_t;
    ivec3 virtual_block_idx=ivec3(samplePos/lod_no_padding_block_length);
    ivec3 lod_block_dim=(block_dim+lod_t-1)/lod_t;

    if(samplePos.x<0.f || virtual_block_idx.x>=lod_block_dim.x
    || samplePos.y<0.f || virtual_block_idx.y>=lod_block_dim.y
    || samplePos.z<0.f || virtual_block_idx.z>=lod_block_dim.z
    ){
        //shader meagings view-space intersect with volume, should return true to keep raycasting
//        scalar=vec4(0.0f);
        return -1;//no need to continue
    }

    //2. get samplePos's offset in the block
    vec3 offset_in_no_padding_block=(samplePos-virtual_block_idx*lod_no_padding_block_length)/lod_t;
    //3. get block-dim in texture
    int flat_virtual_block_idx=virtual_block_idx.z*lod_block_dim.y*lod_block_dim.x
    +virtual_block_idx.y*lod_block_dim.x+virtual_block_idx.x+int(lod_mapping_table_offset[cur_lod]);

    uvec4 physical_block_idx=mapping_table.page_entry[flat_virtual_block_idx];

    //valid block cached in texture
    uint physical_block_flag=((physical_block_idx.w>>16)&uint(0x0000ffff));

    if(physical_block_flag==0){//not need

        scalar=vec4(0.f);
        return 0;
    }
    else if(physical_block_flag==1){//need and loaded
        //4.get sample_pos in the texture
        vec3 pyhsical_sampel_pos=physical_block_idx.xyz*block_length
        +offset_in_no_padding_block+vec3(padding);
        pyhsical_sampel_pos/=texture_size3;
        uint texture_id=physical_block_idx.w&uint(0x0000ffff);
        if(texture_id==0){
            scalar=texture(cache_volume0,pyhsical_sampel_pos);
        }
        else if(texture_id==1){
            scalar=texture(cache_volume1,pyhsical_sampel_pos);
        }
        else if(texture_id==2){
            scalar=texture(cache_volume2,pyhsical_sampel_pos);
        }
        else if(texture_id==3){
            scalar=texture(cache_volume3,pyhsical_sampel_pos);
        }
        else if(texture_id==4){
            scalar=texture(cache_volume4,pyhsical_sampel_pos);
        }
        else{
            scalar=vec4(0.6f);
            return -3;//error texture id
        }
//         scalar=vec4(0.99f);
//         scalar=vec4(pyhsical_sampel_pos,1.f);
        return 1;
    }
    else if(physical_block_flag==2){//need but not loaded
//        scalar=vec4(0.0f);
        return 2;//??? todo
    }
    else{//error flag
         scalar=vec4(0.6f);
        return -2;
    }
}

vec3 phongShading(int lod_t,vec3 samplePos,vec3 diffuseColor)
{
    vec3 N;
    #define CUBIC
    #ifdef CUBIC
    float value[27];
    float t1[9];
    float t2[3];
    for(int k=-1;k<2;k++){//z
        for(int j=-1;j<2;j++){//y
            for(int i=-1;i<2;i++){//x
                vec4 scalar;
                virtualSample(lod_t,samplePos+vec3(i*lod_t,j*lod_t,k*lod_t),scalar);
                value[(k+1)*9+(j+1)*3+i+1]=scalar.r;
            }
        }
    }
    int x,y,z;
    //for x-direction
    for(z=0;z<3;z++){
        for(y=0;y<3;y++){
            t1[z*3+y]=(value[18+y*3+z]-value[y*3+z])/2;
        }
    }
    for(z=0;z<3;z++)
    t2[z]=(t1[z*3+0]+4*t1[z*3+1]+t1[z*3+2])/6;
    N.x=(t2[0]+t2[1]*4+t2[2])/6;


    //for y-direction
    for(z=0;z<3;z++){
        for(x=0;x<3;x++){
            t1[z*3+x]=(value[x*9+6+z]-value[x*9+z])/2;
        }
    }
    for(z=0;z<3;z++)
    t2[z]=(t1[z*3+0]+4*t1[z*3+1]+t1[z*3+2])/6;
    N.y=(t2[0]+t2[1]*4+t2[2])/6;

    //for z-direction
    for(y=0;y<3;y++){
        for(x=0;x<3;x++){
            t1[y*3+x]=(value[x*9+y*3+2]-value[x*9+y*3])/2;
        }
    }
    for(y=0;y<3;y++)
    t2[y]=(t1[y*3+0]+4*t1[y*3+1]+t1[y*3+2])/6;
    N.z=(t2[0]+t2[1]*4+t2[2])/6;
    #else
    //    N.x=value[14]-value[12];
    //    N.y=value[16]-value[10];
    //    N.z=value[22]-value[4];
    //    N.x=(virtualSample(volume_data,samplePos+vec3(step,0,0)).r-virtualSample(volume_data,samplePos+vec3(-step,0,0)).r);
    //    N.y=(virtualSample(volume_data,samplePos+vec3(0,step,0)).r-virtualSample(volume_data,samplePos+vec3(0,-step,0)).r);
    //    N.z=(virtualSample(volume_data,samplePos+vec3(0,0,step)).r-virtualSample(volume_data,samplePos+vec3(0,0,-step)).r);
    #endif

    N=-normalize(N);


    vec3 L=-view_direction;
    vec3 R=-view_direction;//-ray_direction;

    if(dot(N,L)<0.f)
    N=-N;

    vec3 ambient=ka*diffuseColor.rgb;
    vec3 specular=ks*pow(max(dot(N,(L+R)/2.0),0.0),shininess)*vec3(1.0f);
    vec3 diffuse=kd*max(dot(N,L),0.0)*diffuseColor.rgb;
    return ambient+specular+diffuse;
}