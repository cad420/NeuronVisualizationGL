//
// Created by wyz on 2021/4/13.
//

#ifndef NEURONVISUALIZATIONGL_LARGE_VOLUME_RENDERER_H
#define NEURONVISUALIZATIONGL_LARGE_VOLUME_RENDERER_H

#include <Render/shader_program.hpp>
#include <GLFW/glfw3.h>
#include <Common/help_cuda.hpp>
#include <Common/utils.hpp>
#include <Common/boundingbox.hpp>
#include <Common/camera.hpp>
#include <unordered_set>
#include <map>
struct Myhash{
    std::size_t operator()(const sv::AABB& aabb) const {
        return (aabb.index[3]<<24)+(aabb.index[0]<<16)+(aabb.index[1]<<8)+aabb.index[2];
    }
};
class BlockReqInfo;
class LargeVolumeManager;
class LargeVolumeRenderer final{
public:
    LargeVolumeRenderer(int w=1200,int h=900);
    ~LargeVolumeRenderer();
    void set_volume(const char* volume_file);

    void set_transferfunc(std::map<uint8_t,std::array<double,4>>);


    void set_camera(std::tuple<std::array<float,3>,std::array<float,3>,std::array<float,3>,float,float,float>);

    void render();

private:
    void initResourceContext();
    void initGL();
    void initCUDA();

    void setupSystemInfo();

    void setupVolumeInfo();

    void setupControl();

    void createResource();
    void createGLResource();
    void createCUDAResource();
    void createUtilResource();

    //createGLResource
    void createVolumeTexManager();
    void createScreenQuad();
    void createGLTexture();
    void createGLSampler();
    void createGLShader();

    //createCUDAResource
    void createCUgraphics();


    //createUtilResource
    void createVirtualBoxes();
    void createMappingTable();

    //setupRuntimeResource

    void setupRuntimeResource();
    void setupShaderUniform();
    void bindGLTextureUnit();


    //deleteResource
    void deleteResource();
    void deleteGLResource();
    void deleteCUDAResource();
    void deleteUtilResource();

private:
    struct BlockTableItem{
        std::array<uint32_t,4> block_index;
        std::array<uint32_t,4> pos_index;
        bool valid;
        bool cached;
    };
private:
    bool updateCurrentBlocks(const sv::Pyramid& view_pyramid);
    void refineCurrentIntersectBlocks(std::unordered_set<sv::AABB,Myhash>&);
    auto getBlockRequestInfo()->BlockReqInfo;
    void updateNewNeedBlocksInCache();
    void uploadMappingTable();
    bool getTexturePos(const std::array<uint32_t,4>&,std::array<uint32_t,4>&);
    void copyDeviceToTexture(CUdeviceptr,std::array<uint32_t,4>);
    void updateCameraUniform();
private:

    uint32_t block_length,padding,no_padding_block_length;
    std::array<uint32_t,3> block_dim;
    std::unordered_map<int,std::array<uint32_t,4>> lod_block_dim;//lod_block_dim[0][3] represent lod0's total block num
    int min_lod,max_lod;

    //system info
    int max_dedicated_video_mem;
    int total_available_mem;
    int current_available_mem;


    uint32_t vol_tex_block_nx,vol_tex_block_ny,vol_tex_block_nz;
    uint32_t vol_tex_num;//equal to volume texture num
    std::vector<GLuint> volume_texes;
    std::list<BlockTableItem> volume_tex_manager;

//    GLuint intersect_box_ssbo;
//    GLuint* mapped_intersect_box_ptr= nullptr;

    GLuint mapping_table_ssbo;
    std::vector<uint32_t> mapping_table;
    std::vector<uint32_t> lod_mapping_table_offset;

    std::unordered_map<int,std::vector<sv::AABB>> virtual_blocks;//lod1's block_length/lod0's block_length=2
//    GLuint box_vao,box_vbo,box_ebo;

    std::unordered_set<sv::AABB,Myhash> current_blocks;
    std::unordered_set<sv::AABB,Myhash> new_need_blocks,no_need_blocks;
    //some value for debug and imgui
    int aabb_intersect_num,obb_intersect_num,pyramid_intersect_num,refined_intersect_num;


    std::vector<CUgraphicsResource> cu_resources;
    CUcontext cu_context;

    GLuint transfer_func_tex;
    GLuint preInt_tf_tex;

    GLuint gl_sampler;

    std::unique_ptr<LargeVolumeManager> volume_manager;

    uint32_t window_width,window_height;
    GLFWwindow* window;

    GLuint screen_quad_vao;
    GLuint screen_quad_vbo;
    std::array<GLfloat,24> screen_quad_vertices;//6 vertices for x y and u v


    std::unique_ptr<sv::RayCastCamera> camera;

    std::unique_ptr<sv::Shader> raycasting_shader;
    std::unique_ptr<sv::Shader> intersect_shader;

};
#endif //NEURONVISUALIZATIONGL_LARGE_VOLUME_RENDERER_H
