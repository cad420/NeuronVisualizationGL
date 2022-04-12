//
// Created by wyz on 2021/4/13.
//
#include "large_volume_renderer.hpp"
#include <Common/help_gl.hpp>
#include <Data/large_volume_manager.hpp>
#include <Data/transfer_function.hpp>
#include <Render/shaders.hpp>
#include <cudaGL.h>
#include <spdlog/spdlog.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <Window/WindowManager.hpp>

LargeVolumeRenderer::LargeVolumeRenderer(int w, int h)
    : window_width(w), window_height(h), windowManager(WindowManager::Instance())
{
    initResourceContext();
}

LargeVolumeRenderer::~LargeVolumeRenderer()
{
    volume_manager.reset(nullptr);
    deleteResource();
}

void LargeVolumeRenderer::set_volume(const char *volume_file)
{
    this->volume_manager = std::make_unique<LargeVolumeManager>(volume_file, cu_context);
    setupVolumeInfo();
    setupSystemInfo();
    createResource();
}

#define B_TF_TEX_BINDING 0
#define B_PTF_TEX_BINDING 1
#define B_POS_ENTRY_BINDING 2
#define B_POS_EXIT_BINDING 3
#define B_VOL_TEX_0_BINDING 4
#define B_VOL_TEX_1_BINDING 5
#define B_VOL_TEX_2_BINDING 6
#define B_VOL_TEX_3_BINDING 7
#define B_VOL_TEX_4_BINDING 8

void LargeVolumeRenderer::bindGLTextureUnit()
{
    std::cout << __FUNCTION__ << std::endl;
    GL_EXPR(glBindTextureUnit(B_TF_TEX_BINDING, transfer_func_tex));
    GL_EXPR(glBindTextureUnit(B_PTF_TEX_BINDING, preInt_tf_tex));
    GL_EXPR(glBindTextureUnit(B_VOL_TEX_0_BINDING, volume_texes[0]));
    GL_EXPR(glBindTextureUnit(B_VOL_TEX_1_BINDING, volume_texes[1]));
    GL_EXPR(glBindTextureUnit(B_VOL_TEX_2_BINDING, volume_texes[2]));
    //    GL_EXPR(glBindTextureUnit(B_VOL_TEX_3_BINDING,volume_texes[3]));
    //    GL_EXPR(glBindTextureUnit(B_VOL_TEX_4_BINDING,volume_texes[4]));

    glBindSampler(B_VOL_TEX_0_BINDING, gl_sampler);
    glBindSampler(B_VOL_TEX_1_BINDING, gl_sampler);
    glBindSampler(B_VOL_TEX_2_BINDING, gl_sampler);
    glBindSampler(B_VOL_TEX_3_BINDING, gl_sampler);
    //    glBindSampler(B_VOL_TEX_4_BINDING,gl_sampler);

    GL_CHECK
}

void LargeVolumeRenderer::setupShaderUniform()
{
    std::cout << __FUNCTION__ << std::endl;
    raycasting_shader->use();
    auto tmp = lod_mapping_table_offset;
    for (int i = 0; i < tmp.size(); i++)
        tmp[i] /= 4;
    raycasting_shader->setUIArray("lod_mapping_table_offset", tmp.data(), tmp.size());
    raycasting_shader->setUInt("min_lod", min_lod);
    raycasting_shader->setUInt("max_lod", max_lod);

    raycasting_shader->setInt("transfer_func", B_TF_TEX_BINDING);
    raycasting_shader->setInt("preInt_transfer_func", B_PTF_TEX_BINDING);

    raycasting_shader->setInt("cache_volume0", B_VOL_TEX_0_BINDING);
    raycasting_shader->setInt("cache_volume1", B_VOL_TEX_1_BINDING);
    raycasting_shader->setInt("cache_volume2", B_VOL_TEX_2_BINDING);
    raycasting_shader->setInt("cache_volume3", B_VOL_TEX_3_BINDING);
    //    raycasting_shader->setInt("cache_volume4",B_VOL_TEX_4_BINDING);

    raycasting_shader->setVec4("bg_color", 0.f, 0.f, 0.f, 0.f);
    raycasting_shader->setInt("block_length", block_length);
    raycasting_shader->setInt("padding", padding);
    raycasting_shader->setIVec3("block_dim", block_dim[0], block_dim[1], block_dim[2]);
    raycasting_shader->setIVec3("texture_size3", vol_tex_block_nx * block_length, vol_tex_block_ny * block_length,
                                vol_tex_block_nz * block_length);
    raycasting_shader->setVec3("board_length", volume_board[0], volume_board[1], volume_board[1]);

    raycasting_shader->setFloat("ka", 0.5f);
    raycasting_shader->setFloat("kd", 0.8f);
    raycasting_shader->setFloat("shininess", 100.0f);
    raycasting_shader->setFloat("ks", 0.3f);
    raycasting_shader->setVec3("light_direction", glm::normalize(glm::vec3(-1.0f, -1.0f, -1.0f)));

    raycasting_shader->setUInt("window_width", window_width);
    raycasting_shader->setUInt("window_height", window_height);
    raycasting_shader->setVec3("camera_pos", camera->camera_pos);
    raycasting_shader->setVec3("view_pos", camera->camera_pos + camera->n * camera->view_direction);
    raycasting_shader->setVec3("view_direction", camera->view_direction);
    raycasting_shader->setVec3("view_right", camera->right);
    raycasting_shader->setVec3("view_up", camera->up);
    raycasting_shader->setFloat("view_depth", camera->f);
    raycasting_shader->setFloat("view_right_space", camera->space_x);
    raycasting_shader->setFloat("view_up_space", camera->space_y);
    raycasting_shader->setFloat("step", camera->space_z);
}

void LargeVolumeRenderer::setupRuntimeResource()
{
    bindGLTextureUnit();
    setupShaderUniform();
}

std::function<void(GLFWwindow *, float)> process_input;
void LargeVolumeRenderer::render()
{
    setupControl();

    setupRuntimeResource();

    double last_frame_t = 0.0;

    std::cout << "start render" << std::endl;
    while (!glfwWindowShouldClose(window))
    {

        double current_frame_t = glfwGetTime();
        double delta_t = current_frame_t - last_frame_t;
        last_frame_t = current_frame_t;

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

        glfwPollEvents();
        process_input(window, delta_t);

        bool intersect;
        START_CPU_TIMER
        intersect = updateCurrentBlocks(camera->getPyramid());
        END_CPU_TIMER

        if (intersect)
        {
            updateNewNeedBlocksInCache();

            volume_manager->setupBlockReqInfo(getBlockRequestInfo());

            BlockDesc block;
            while (volume_manager->getBlock(block))
            {

                if (isValidBlock(block.block_index))
                {

                    copyDeviceToTexture(block.data_ptr, block.block_index);
                }

                volume_manager->updateCUMemPool(block.data_ptr);

                block.release();
            }
        }

        uploadMappingTable();

        updateCameraUniform();

        bool is_inside = false;
        // render volume
        {
            glm::mat4 view = camera->getViewMatrix();

            glm::mat4 projection =
                glm::perspective(glm::radians(camera->fov), (float)window_width / (float)window_height, 1.f, 50000.0f);
            int colNum = windowManager.GetWindowColSize();
            int rowNum = windowManager.GetWindowRowSize();
            int col = windowManager.GetTileWindowOffsetX();
            int row = windowManager.GetTileWindowOffsetY();

            projection[0][0] *= colNum;
            projection[1][1] *= rowNum;
            projection[2][0] = -colNum + 1 + 2 * col;
            projection[2][1] = rowNum - 1 - 2 * row;

            glm::mat4 mvp = projection * view;
            raycast_pos_shader->use();
            raycast_pos_shader->setMat4("MVPMatrix", mvp);
            raycast_pos_shader->setVec3("camera_pos", camera->camera_pos);
            raycast_pos_shader->setVec3("board_length", volume_board[0], volume_board[1], volume_board[2]);

            if (camera->camera_pos.x < volume_board[0] && camera->camera_pos.y < volume_board[1] &&
                camera->camera_pos.z < volume_board[2] && camera->camera_pos.x > 0.f && camera->camera_pos.y > 0.f &&
                camera->camera_pos.z > 0.f)
            {

                is_inside = true;
            }
            raycast_pos_shader->setBool("inside", is_inside);
            glBindFramebuffer(GL_FRAMEBUFFER, raycast_pos_fbo);
            glBindVertexArray(volume_vao);
            glDrawBuffer(GL_COLOR_ATTACHMENT0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            raycast_pos_shader->setBool("entry", true);
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

            glEnable(GL_CULL_FACE);
            glFrontFace(GL_CCW);
            glDrawBuffer(GL_COLOR_ATTACHMENT1);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            raycast_pos_shader->setBool("entry", false);
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
            glDisable(GL_CULL_FACE);

            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            raycasting_shader->use();

            glBindVertexArray(screen_quad_vao);

            glDrawArrays(GL_TRIANGLES, 0, 6);
            GL_CHECK
        }

        // render imgui
        {
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            {
                ImGui::Begin("Large Volume Renderer");
                ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
                ImGui::Text("volume board %f %f %f", volume_board[0], volume_board[1], volume_board[2]);
                ImGui::InputFloat3("camera pos", &camera->camera_pos.x);
                ImGui::InputFloat3("view direction", &camera->view_direction.x);
                ImGui::InputFloat("camera fov", &camera->fov);
                ImGui::InputFloat("camera f", &camera->f);
                ImGui::Text("inside board: %d", is_inside);
                //                ImGui::SliderFloat("camera f",&camera->f,512.f,4096.f);
                ImGui::SliderFloat("step", &camera->space_z, 0.f, 1.f);
                ImGui::Text("intersect num: aabb(%d), obb(%d), pyramid(%d)", aabb_intersect_num, obb_intersect_num,
                            pyramid_intersect_num);
                ImGui::Text("preload pyramid intersect num: %d", preload_pyramid_intersect_num);
                ImGui::Text("refined intersect num: %d", refined_intersect_num);
                ImGui::Text("max_dedicated_video_mem: %d GB", max_dedicated_video_mem >> 20);
                ImGui::Text("total_available_mem: %d GB", total_available_mem >> 20);
                ImGui::Text("current_available_mem: %d GB", current_available_mem >> 20);
                ImGui::End();
            }
            ImGui::EndFrame();
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        }

        MPI_Barrier(MPI_COMM_WORLD);
        glfwSwapBuffers(window);

        while (glfwGetTime() < last_frame_t + 1.0 / 15)
        {
        }
    }
}

void LargeVolumeRenderer::updateCameraUniform()
{
    raycasting_shader->setVec3("camera_pos", camera->camera_pos);
    raycasting_shader->setVec3("view_pos", camera->camera_pos + camera->n * camera->view_direction);
    raycasting_shader->setVec3("view_direction", camera->view_direction);
    raycasting_shader->setVec3("view_right", camera->right);
    raycasting_shader->setVec3("view_up", camera->up);
    raycasting_shader->setFloat("view_depth", camera->f);
    raycasting_shader->setFloat("view_right_space", camera->space_x);
    raycasting_shader->setFloat("view_up_space", camera->space_y);
    raycasting_shader->setFloat("step", camera->space_z);
}

void LargeVolumeRenderer::createVirtualBoxes()
{
    for (uint32_t i = min_lod; i <= max_lod; i++)
    {
        uint32_t t = std::pow(2, (i - min_lod));
        uint32_t dim_x = lod_block_dim[i][0]; //(block_dim[0]+t-1)/t;
        uint32_t dim_y = lod_block_dim[i][1]; //(block_dim[1]+t-1)/t;
        uint32_t dim_z = lod_block_dim[i][2]; //(block_dim[2]+t-1)/t;
        uint32_t lod_no_padding_block_length = no_padding_block_length * t;
        for (uint32_t z = 0; z < dim_z; z++)
        {
            for (uint32_t y = 0; y < dim_y; y++)
            {
                for (uint32_t x = 0; x < dim_x; x++)
                {
                    virtual_blocks[i].emplace_back(
                        glm::vec3(x * lod_no_padding_block_length, y * lod_no_padding_block_length,
                                  z * lod_no_padding_block_length),
                        glm::vec3((x + 1) * lod_no_padding_block_length, (y + 1) * lod_no_padding_block_length,
                                  (z + 1) * lod_no_padding_block_length),
                        std::array<uint32_t, 4>{x, y, z, i});
                }
            }
        }
    }
}

void LargeVolumeRenderer::createVolumeBoard()
{
    float board_x = lod_block_dim[min_lod][0] * no_padding_block_length;
    float board_y = lod_block_dim[min_lod][1] * no_padding_block_length;
    float board_z = lod_block_dim[min_lod][2] * no_padding_block_length;
    volume_board = {board_x, board_y, board_z};
    std::cout << "max board: " << board_x << " " << board_y << " " << board_z << std::endl;
    std::array<std::array<float, 3>, 8> board_cube;
    board_cube[0] = {0.0f, 0.0f, 0.0f};
    board_cube[1] = {board_x, 0.0f, 0.0f};
    board_cube[2] = {board_x, board_y, 0.0f};
    board_cube[3] = {0.0f, board_y, 0.0f};
    board_cube[4] = {0.0f, 0.0f, board_z};
    board_cube[5] = {board_x, 0.0f, board_z};
    board_cube[6] = {board_x, board_y, board_z};
    board_cube[7] = {0.0f, board_y, board_z};
    std::array<GLuint, 36> board_cube_indices = {0, 1, 2, 0, 2, 3, 0, 4, 1, 4, 5, 1, 1, 5, 6, 6, 2, 1,
                                                 6, 7, 2, 7, 3, 2, 7, 4, 3, 3, 4, 0, 4, 7, 6, 4, 6, 5};

    glGenVertexArrays(1, &volume_vao);
    glGenBuffers(1, &volume_vbo);
    glGenBuffers(1, &volume_ebo);
    glBindVertexArray(volume_vao);
    glBindBuffer(GL_ARRAY_BUFFER, volume_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(board_cube), board_cube.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, volume_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(board_cube_indices), board_cube_indices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void LargeVolumeRenderer::createGLTexture()
{
    assert(block_length && vol_tex_block_nx && vol_tex_block_ny && vol_tex_block_nz && vol_tex_num);
    assert(volume_texes.size() == 0);
    volume_texes.assign(vol_tex_num, 0);
    glCreateTextures(GL_TEXTURE_3D, vol_tex_num, volume_texes.data());
    for (int i = 0; i < vol_tex_num; i++)
    {
        glTextureStorage3D(volume_texes[i], 1, GL_R8, vol_tex_block_nx * block_length, vol_tex_block_ny * block_length,
                           vol_tex_block_nz * block_length);
    }
    GL_CHECK
}

void LargeVolumeRenderer::createVolumeTexManager()
{
    assert(vol_tex_num == volume_texes.size());
    for (uint32_t t = 0; t < vol_tex_num; t++)
    {
        for (uint32_t i = 0; i < vol_tex_block_nx; i++)
        {
            for (uint32_t j = 0; j < vol_tex_block_ny; j++)
            {
                for (uint32_t k = 0; k < vol_tex_block_nz; k++)
                {
                    BlockTableItem item;
                    item.pos_index = {i, j, k, t};
                    item.block_index = {INVALID, INVALID, INVALID, INVALID};
                    item.valid = false;
                    item.cached = false;
                    volume_tex_manager.push_back(item);
                }
            }
        }
    }
}

void LargeVolumeRenderer::refineCurrentIntersectBlocks(std::unordered_set<sv::AABB, Myhash> &blocks)
{

    std::unordered_set<sv::AABB, Myhash> refined_blocks;
    for (auto &block : blocks)
    {
        auto block_pos = (block.max_p + block.min_p) / 2.f;
        auto view_pos = camera->camera_pos;
        float dist = glm::length(block_pos - view_pos);

        uint32_t dist_lod = std::sqrt(dist / no_padding_block_length) + min_lod;
        dist_lod = dist_lod > max_lod ? max_lod : dist_lod;
        assert(block.index[3] == min_lod);
        int t = std::pow(2, dist_lod - min_lod);
        uint32_t idx_x = block.index[0] / t;
        uint32_t idx_y = block.index[1] / t;
        uint32_t idx_z = block.index[2] / t;
        uint32_t lod_no_padding_block_length = no_padding_block_length * t;
        refined_blocks.insert(
            {glm::vec3(idx_x * lod_no_padding_block_length, idx_y * lod_no_padding_block_length,
                       idx_z * lod_no_padding_block_length),
             glm::vec3((idx_x + 1) * lod_no_padding_block_length, (idx_y + 1) * lod_no_padding_block_length,
                       (idx_z + 1) * lod_no_padding_block_length),
             std::array<uint32_t, 4>{idx_x, idx_y, idx_z, dist_lod}});
    }
    blocks.swap(refined_blocks);
}

bool LargeVolumeRenderer::updateCurrentBlocks(const sv::Pyramid &view_pyramid)
{
    auto view_obb = view_pyramid.getOBB();
    auto view_aabb = view_obb.getAABB();

    std::unordered_set<sv::AABB, Myhash> current_aabb_intersect_blocks;
    std::unordered_set<sv::AABB, Myhash> current_obb_intersect_blocks;
    std::unordered_set<sv::AABB, Myhash> current_pyramid_intersect_blocks;
    std::unordered_set<sv::AABB, Myhash> preload_pyramid_intersect_blocks;

    //计算与当前相机视锥体所代表的AABB相交的数据块
    for (auto &it : virtual_blocks[min_lod])
    {
        if (view_aabb.intersect(it))
        {
            current_aabb_intersect_blocks.insert(it);
        }
    }

    aabb_intersect_num = current_aabb_intersect_blocks.size();

    //进一步计算与视锥体代表的OBB相交的数据块
    for (auto &it : current_aabb_intersect_blocks)
    {
        if (view_obb.intersect_obb(it.convertToOBB()))
        {
            current_obb_intersect_blocks.insert(it);
        }
    }

    obb_intersect_num = current_obb_intersect_blocks.size();

    //进一步计算与视锥体相交的数据块
    for (auto &it : current_obb_intersect_blocks)
    {
        if (view_pyramid.intersect_aabb(it))
        {
            current_pyramid_intersect_blocks.insert(it);
        }
    }

    pyramid_intersect_num = current_pyramid_intersect_blocks.size();

    //根据混合分辨策略 由lod0相交的块转换为各个lod所需的数据块
    refineCurrentIntersectBlocks(current_pyramid_intersect_blocks);

    refined_intersect_num = current_pyramid_intersect_blocks.size();

    if (current_pyramid_intersect_blocks.empty())
    {
        // no intersect, so no need to draw
        return false;
    }

    assert(new_need_blocks.empty());
    assert(no_need_blocks.empty());

    //更新当前帧新需要的数据块 相对于上一帧不需要的数据块 以及当前帧绘制需要的数据块

    for (auto &it : current_pyramid_intersect_blocks)
    {
        if (current_blocks.find(it) == current_blocks.cend())
        {
            new_need_blocks.insert(it);
        }
    }
    new_need_block_num = new_need_blocks.size();
    for (auto &it : current_blocks)
    {
        if (current_pyramid_intersect_blocks.find(it) == current_pyramid_intersect_blocks.cend())
        {
            no_need_blocks.insert(it);
        }
    }
    no_need_block_num = no_need_blocks.size();

    current_blocks = std::move(current_pyramid_intersect_blocks);

    //将当前帧相对于上一帧不再需要的数据块的页表项valid设为false

    for (auto &it : volume_tex_manager)
    {
        auto t = sv::AABB(it.block_index);
        // not find
        if (no_need_blocks.find(t) != no_need_blocks.cend())
        {
            it.valid = false;
        }
        else
        {
        }
    }

    return true;
}

auto LargeVolumeRenderer::getBlockRequestInfo() -> BlockReqInfo
{
    BlockReqInfo request;
    for (auto &it : new_need_blocks)
    {
        request.request_blocks_queue.push_back(it.index);
    }
    for (auto &it : no_need_blocks)
    {
        request.noneed_blocks_queue.push_back(it.index);
    }

    new_need_blocks.clear();
    no_need_blocks.clear();
    return request;
}

void LargeVolumeRenderer::updateNewNeedBlocksInCache()
{
    //如果所需的数据块已经加载到显存了 则将其从new_need_blocks中删除 并修改对应的页表项为valid
    for (auto &it : volume_tex_manager)
    {
        // find cached but invalid block in texture
        if (it.cached && !it.valid)
        {
            auto t = sv::AABB(it.block_index);
            if (new_need_blocks.find(t) != new_need_blocks.cend())
            { // find!
                it.valid = true;

                uint32_t flat_idx =
                    (it.block_index[2] * lod_block_dim[it.block_index[3]][0] * lod_block_dim[it.block_index[3]][1] +
                     it.block_index[1] * lod_block_dim[it.block_index[3]][0] + it.block_index[0]) *
                        4 +
                    lod_mapping_table_offset[it.block_index[3]];
                mapping_table[flat_idx + 0] = it.pos_index[0];
                mapping_table[flat_idx + 1] = it.pos_index[1];
                mapping_table[flat_idx + 2] = it.pos_index[2];
                mapping_table[flat_idx + 3] = it.pos_index[3] | (0x00010000); // really needed and loaded

                new_need_blocks.erase(t);
            }
        }
    }
}

bool LargeVolumeRenderer::isValidBlock(std::array<uint32_t, 4> idx)
{
    for (auto &it : current_blocks)
    {
        if (it.index == idx)
        {
            return true;
        }
    }
    return false;
}

void LargeVolumeRenderer::copyDeviceToTexture(CUdeviceptr ptr, std::array<uint32_t, 4> idx)
{
    spdlog::info("{0}", __FUNCTION__);

    std::array<uint32_t, 4> tex_pos_index;
    bool cached = getTexturePos(idx, tex_pos_index);

    if (!cached)
    {
        assert(tex_pos_index[3] < vol_tex_num && tex_pos_index[0] < vol_tex_block_nx &&
               tex_pos_index[1] < vol_tex_block_ny && tex_pos_index[2] < vol_tex_block_nz);

        CUDA_DRIVER_API_CALL(cuGraphicsMapResources(1, &cu_resources[tex_pos_index[3]], 0));

        CUarray cu_array;

        CUDA_DRIVER_API_CALL(cuGraphicsSubResourceGetMappedArray(&cu_array, cu_resources[tex_pos_index[3]], 0, 0));

        CUDA_MEMCPY3D m = {0};
        m.srcMemoryType = CU_MEMORYTYPE_DEVICE;
        m.srcDevice = ptr;

        m.dstMemoryType = CU_MEMORYTYPE_ARRAY;
        m.dstArray = cu_array;
        m.dstXInBytes = tex_pos_index[0] * block_length;
        m.dstY = tex_pos_index[1] * block_length;
        m.dstZ = tex_pos_index[2] * block_length;

        m.WidthInBytes = block_length;
        m.Height = block_length;
        m.Depth = block_length;

        CUDA_DRIVER_API_CALL(cuMemcpy3D(&m));

        CUDA_DRIVER_API_CALL(cuGraphicsUnmapResources(1, &cu_resources[tex_pos_index[3]], 0));

        spdlog::info("finish cuda opengl copy");
    }
    // update volume_tex_manager

    for (auto &it : volume_tex_manager)
    {
        if (it.pos_index == tex_pos_index)
        {
            if (cached)
            {
                assert(it.cached == true && it.block_index == idx);
            }
            if (it.block_index != idx && it.block_index[0] != INVALID)
            { // set mapping_table flag 0
                uint32_t flat_idx =
                    (it.block_index[2] * lod_block_dim[it.block_index[3]][0] * lod_block_dim[it.block_index[3]][1] +
                     it.block_index[1] * lod_block_dim[it.block_index[3]][0] + it.block_index[0]) *
                        4 +
                    lod_mapping_table_offset[it.block_index[3]];
                mapping_table[flat_idx + 3] &= 0x0000ffff;
            }
            it.block_index = idx;
            it.valid = true;
            it.cached = true;
        }
    }

    // update mapping_table
    uint32_t flat_idx =
        (idx[2] * lod_block_dim[idx[3]][0] * lod_block_dim[idx[3]][1] + idx[1] * lod_block_dim[idx[3]][0] + idx[0]) *
            4 +
        lod_mapping_table_offset[idx[3]];
    mapping_table[flat_idx + 0] = tex_pos_index[0];
    mapping_table[flat_idx + 1] = tex_pos_index[1];
    mapping_table[flat_idx + 2] = tex_pos_index[2];
    mapping_table[flat_idx + 3] = tex_pos_index[3] | (0x00010000);
}

bool LargeVolumeRenderer::getTexturePos(const std::array<uint32_t, 4> &idx, std::array<uint32_t, 4> &pos)
{
    for (auto &it : volume_tex_manager)
    {
        if (it.block_index == idx && it.cached && !it.valid)
        {
            pos = it.pos_index;
            return true;
        }
    }
    for (auto &it : volume_tex_manager)
    {
        if (!it.valid && !it.cached)
        {
            pos = it.pos_index;
            return false;
        }
    }
    for (auto &it : volume_tex_manager)
    {
        if (!it.valid)
        {
            pos = it.pos_index;
            return false;
        }
    }
    spdlog::critical("not find empty texture pos");
    throw std::runtime_error("not find empty texture pos");
}

void LargeVolumeRenderer::uploadMappingTable()
{
    GL_EXPR(glNamedBufferSubData(mapping_table_ssbo, 0, mapping_table.size() * sizeof(uint32_t), mapping_table.data()));
}

void LargeVolumeRenderer::createMappingTable()
{
    assert(max_lod < 9);
    lod_mapping_table_offset.assign(10, 0);
    for (int i = min_lod; i <= max_lod; i++)
    {
        auto lod_block_num = lod_block_dim[i][3];
        lod_mapping_table_offset[i + 1] = lod_mapping_table_offset[i] + lod_block_num * 4;
    }
    mapping_table.assign(lod_mapping_table_offset[max_lod + 1], 0);
    glGenBuffers(1, &mapping_table_ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, mapping_table_ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, mapping_table.size() * sizeof(uint32_t), mapping_table.data(),
                 GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, mapping_table_ssbo);
    GL_CHECK
}

void LargeVolumeRenderer::setupSystemInfo()
{
    constexpr int GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX = 0x9047;
    constexpr int GPU_MEMORY_INFO_TOTAL_AVAILABEL_MEMORY_NVX = 0x9048;
    constexpr int GPU_MEMORY_INFO_CURRENT_AVAILABEL_VIDEMEM_NVX = 0x9049;
    GL_EXPR(glGetIntegerv(GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX, &max_dedicated_video_mem));
    GL_EXPR(glGetIntegerv(GPU_MEMORY_INFO_TOTAL_AVAILABEL_MEMORY_NVX, &total_available_mem));
    GL_EXPR(glGetIntegerv(GPU_MEMORY_INFO_CURRENT_AVAILABEL_VIDEMEM_NVX, &current_available_mem));

    vol_tex_block_nx = 4;
    vol_tex_block_ny = 2;
    vol_tex_block_nz = 2;
    vol_tex_num = 5;
    uint64_t single_texture_size = ((uint64_t)vol_tex_block_nx * vol_tex_block_ny * vol_tex_block_nz * block_length *
                                    block_length * block_length) /
                                   1024;
    std::cout << "single_texture_size: " << single_texture_size << std::endl;
    if (single_texture_size * vol_tex_num > current_available_mem)
    {
        throw std::runtime_error("current gpu memory not enough");
    }
}

void LargeVolumeRenderer::initResourceContext()
{
    initGL();
    initCUDA();
}

void LargeVolumeRenderer::setupVolumeInfo()
{
    auto volume_info = this->volume_manager->getVolumeInfo();
    this->block_length = std::get<0>(volume_info);
    this->padding = std::get<1>(volume_info);
    this->min_lod = std::get<2>(volume_info);
    this->max_lod = std::get<3>(volume_info);
    this->block_dim = std::get<4>(volume_info);
    this->no_padding_block_length = this->block_length - 2 * this->padding;
    for (int lod = min_lod; lod <= max_lod; lod++)
    {
        uint32_t t = std::pow(2, (lod - min_lod));
        uint32_t dim_x = (block_dim[0] + t - 1) / t;
        uint32_t dim_y = (block_dim[1] + t - 1) / t;
        uint32_t dim_z = (block_dim[2] + t - 1) / t;
        this->lod_block_dim[lod] = {dim_x, dim_y, dim_z, dim_x * dim_y * dim_z};
    }
}

void LargeVolumeRenderer::initGL()
{
    spdlog::info("{0}", __FUNCTION__);
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_DECORATED, GL_FALSE);

    window = glfwCreateWindow(windowManager.GetTileWindowWidth(), windowManager.GetTileWindowHeight(),
                              "Large Volume Render", NULL, NULL);
    if (window == nullptr)
    {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwSetWindowPos(window, windowManager.GetWindowLocationOffsetX(), windowManager.GetWindowLocationOffsetY());

    glfwMakeContextCurrent(window);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    //    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        glfwTerminate();
        throw std::runtime_error("Failed to initialize GLAD");
    }

    glEnable(GL_DEPTH_TEST);

    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();

        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init();
    }
}

void LargeVolumeRenderer::initCUDA()
{
    CUDA_DRIVER_API_CALL(cuInit(0));
    CUdevice cuDevice = 0;
    auto iGPU = windowManager.GetWindowIndex() % windowManager.GetGPUNumPerHost();
    CUDA_DRIVER_API_CALL(cuDeviceGet(&cuDevice, iGPU));
    CUDA_DRIVER_API_CALL(cuCtxCreate(&cu_context, 0, cuDevice));
}

void LargeVolumeRenderer::createResource()
{
    createUtilResource();
    createGLResource();
    createCUDAResource();
}

void LargeVolumeRenderer::createGLResource()
{

    createGLTexture();
    createGLSampler();

    createVolumeTexManager();
    createMappingTable();
    createScreenQuad();
    createVolumeBoard();
    createPosFrameBuffer();
    createGLShader();
}

void LargeVolumeRenderer::createCUDAResource()
{
    createCUgraphics();
}

void LargeVolumeRenderer::createCUgraphics()
{
    spdlog::info("{0}", __FUNCTION__);
    assert(vol_tex_num == volume_texes.size() && vol_tex_num != 0);
    cu_resources.resize(volume_texes.size());
    for (int i = 0; i < volume_texes.size(); i++)
    {
        std::cout << "cuda register " << i << " : " << volume_texes[i] << std::endl;
        // max texture memory is 3GB for register cuda image for rtx3090
        CUDA_DRIVER_API_CALL(cuGraphicsGLRegisterImage(&cu_resources[i], volume_texes[i], GL_TEXTURE_3D,
                                                       CU_GRAPHICS_REGISTER_FLAGS_WRITE_DISCARD));
    }
}

void LargeVolumeRenderer::createUtilResource()
{
    createVirtualBoxes();
}

void LargeVolumeRenderer::createScreenQuad()
{
    spdlog::info("{0}", __FUNCTION__);
    screen_quad_vertices = {-1.0f, 1.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, 0.0f,

                            -1.0f, 1.0f, 0.0f, 1.0f, 1.0f,  -1.0f, 1.0f, 0.0f, 1.0f, 1.0f,  1.0f, 1.0f};

    glGenVertexArrays(1, &screen_quad_vao);
    glGenBuffers(1, &screen_quad_vbo);
    glBindVertexArray(screen_quad_vao);
    glBindBuffer(GL_ARRAY_BUFFER, screen_quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(screen_quad_vertices), screen_quad_vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

void LargeVolumeRenderer::createPosFrameBuffer()
{
    glGenFramebuffers(1, &raycast_pos_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, raycast_pos_fbo);

    glGenTextures(1, &raycast_entry_tex);
    glBindTexture(GL_TEXTURE_2D, raycast_entry_tex);
    glBindTextureUnit(B_POS_ENTRY_BINDING, raycast_entry_tex);
    glTextureStorage2D(raycast_entry_tex, 1, GL_RGBA32F, window_width, window_height);
    glBindImageTexture(0, raycast_entry_tex, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, raycast_entry_tex, 0);

    GL_CHECK;

    glGenRenderbuffers(1, &raycast_pos_rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, raycast_pos_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, window_width, window_height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, raycast_pos_rbo);

    glGenTextures(1, &raycast_exit_tex);
    glBindTexture(GL_TEXTURE_2D, raycast_exit_tex);
    glBindTextureUnit(B_POS_EXIT_BINDING, raycast_exit_tex);
    glTextureStorage2D(raycast_exit_tex, 1, GL_RGBA32F, window_width, window_height);
    glBindImageTexture(1, raycast_exit_tex, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, raycast_exit_tex, 0);

    GL_CHECK

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        throw std::runtime_error("Framebuffer object is not complete!");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    GL_CHECK
}

void LargeVolumeRenderer::set_transferfunc(std::map<uint8_t, std::array<double, 4>> color_setting)
{
    sv::TransferFunc tf(color_setting);
    glGenTextures(1, &transfer_func_tex);
    glBindTexture(GL_TEXTURE_1D, transfer_func_tex);
    //    glCreateTextures(GL_TEXTURE_1D,1,&transfer_func_tex);
    //    GL_EXPR(glBindTextureUnit(B_TF_TEX_BINDING,transfer_func_tex));
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA, TF_DIM, 0, GL_RGBA, GL_FLOAT, tf.getTransferFunction().data());

    glGenTextures(1, &preInt_tf_tex);
    glBindTexture(GL_TEXTURE_2D, preInt_tf_tex);
    //    glBindTextureUnit(B_PTF_TEX_BINDING,preInt_tf_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TF_DIM, TF_DIM, 0, GL_RGBA, GL_FLOAT, tf.getPreIntTransferFunc().data());
}

void LargeVolumeRenderer::set_camera(
    std::tuple<std::array<float, 3>, std::array<float, 3>, std::array<float, 3>, float, float, float> pack)
{
    auto pos = std::get<0>(pack);
    auto front = std::get<1>(pack);
    auto up = std::get<2>(pack);
    auto zoom = std::get<3>(pack);
    auto n = std::get<4>(pack);
    auto f = std::get<5>(pack);
    glm::vec3 camera_pos = {pos[0], pos[1], pos[2]};
    glm::vec3 camera_front = {front[0], front[1], front[2]};
    glm::vec3 camera_up = {up[0], up[1], up[2]};
    this->camera = std::make_unique<sv::RayCastCamera>(camera_pos, camera_front, camera_up, zoom, n, f,
                                                       window_width / 2, window_height / 2);
}

void LargeVolumeRenderer::createGLSampler()
{
    GL_EXPR(glCreateSamplers(1, &gl_sampler));
    GL_EXPR(glSamplerParameterf(gl_sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL_EXPR(glSamplerParameterf(gl_sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    float color[4] = {0.f, 0.f, 0.f, 0.f};
    glSamplerParameterf(gl_sampler, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
    glSamplerParameterf(gl_sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glSamplerParameterf(gl_sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

    GL_EXPR(glSamplerParameterfv(gl_sampler, GL_TEXTURE_BORDER_COLOR, color));
}

void LargeVolumeRenderer::createGLShader()
{
    raycast_pos_shader = std::make_unique<sv::Shader>();
    raycast_pos_shader->setShader(shader::raycast_pos_v, shader::raycast_pos_f);
    raycasting_shader = std::make_unique<sv::Shader>();
    raycasting_shader->setShader(shader::mix_block_raycast_v, shader::mix_block_raycast_f);
}

void LargeVolumeRenderer::deleteResource()
{
    deleteCUDAResource();
    deleteGLResource();
    deleteUtilResource();
}

void LargeVolumeRenderer::deleteGLResource()
{
    glDeleteBuffers(1, &mapping_table_ssbo);
    glDeleteSamplers(1, &gl_sampler);
    glDeleteTextures(1, &transfer_func_tex);
    glDeleteTextures(1, &preInt_tf_tex);
    glDeleteVertexArrays(1, &screen_quad_vao);
    glDeleteBuffers(1, &screen_quad_vbo);
    GL_EXPR(glDeleteTextures(volume_texes.size(), volume_texes.data()));

    GL_CHECK
    glfwTerminate();
}

void LargeVolumeRenderer::deleteCUDAResource()
{
    spdlog::info("{0}", __FUNCTION__);
    assert(volume_texes.size() == cu_resources.size() && vol_tex_num == volume_texes.size() && vol_tex_num != 0);
    for (int i = 0; i < cu_resources.size(); i++)
    {
        CUDA_DRIVER_API_CALL(cuGraphicsUnregisterResource(cu_resources[i]));
    }
    CUDA_DRIVER_API_CALL(cuCtxDestroy(cu_context));
}

void LargeVolumeRenderer::deleteUtilResource()
{
}

std::function<void(GLFWwindow *window, int width, int height)> framebuffer_resize_callback;
void glfw_framebuffer_resize_callback(GLFWwindow *window, int width, int height)
{
    framebuffer_resize_callback(window, width, height);
}
std::function<void(GLFWwindow *window, int button, int action, int mods)> mouse_button_callback;
void glfw_mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
{
    mouse_button_callback(window, button, action, mods);
}
std::function<void(GLFWwindow *window, double xpos, double ypos)> mouse_move_callback;
void glfw_mouse_move_callback(GLFWwindow *window, double xpos, double ypos)
{
    mouse_move_callback(window, xpos, ypos);
}
std::function<void(GLFWwindow *window, double xoffset, double yoffset)> scroll_callback;
void glfw_scroll_callback(GLFWwindow *window, double xoffset, double yoffset)
{
    scroll_callback(window, xoffset, yoffset);
}
std::function<void(GLFWwindow *window, int key, int scancode, int action, int mods)> keyboard_callback;
void glfw_keyboard_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    keyboard_callback(window, key, scancode, action, mods);
}

void LargeVolumeRenderer::setupControl()
{
    framebuffer_resize_callback = [&](GLFWwindow *window, int width, int height) {

    };
    mouse_button_callback = [&](GLFWwindow *window, int button, int action, int mods) {
        if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
        {

            print("mouse button left pressed");
        }
        else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
        {
        }
    };
    mouse_move_callback = [&](GLFWwindow *window, double xpos, double ypos) {
        static double last_x, last_y;
        static bool first = true;
        if (first)
        {
            first = false;
            last_x = xpos;
            last_y = ypos;
        }
        double delta_x = xpos - last_x;
        double delta_y = last_y - ypos;
        last_x = xpos;
        last_y = ypos;
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
        {
        }
        else if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
        {
            camera->processMouseMove(delta_x, delta_y);
        }
    };
    scroll_callback = [&](GLFWwindow *window, double xoffset, double yoffset) { camera->processMouseScroll(yoffset); };
    keyboard_callback = [&](GLFWwindow *window, int key, int scancode, int action, int mods) {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, true);
        }
        if (key == GLFW_KEY_F && action == GLFW_PRESS)
        {
            camera->processKeyForArg(sv::CameraOperation::MOVE_FASTER);
        }
        else if (key == GLFW_KEY_G && action == GLFW_PRESS)
        {
            camera->processKeyForArg(sv::CameraOperation::MOVE_SLOWER);
        }
    };
    glfwSetFramebufferSizeCallback(window, glfw_framebuffer_resize_callback);
    glfwSetMouseButtonCallback(window, glfw_mouse_button_callback);
    glfwSetCursorPosCallback(window, glfw_mouse_move_callback);
    glfwSetScrollCallback(window, glfw_scroll_callback);
    glfwSetKeyCallback(window, glfw_keyboard_callback);

    process_input = [&](GLFWwindow *window, float delta_time) {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, true);
        }
        sv::CameraOperation operation = sv::CameraOperation::NONE;
        if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS)
        {
            operation = sv::CameraOperation::MOVE_FASTER;
        }
        if (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS)
        {
            operation = sv::CameraOperation::MOVE_SLOWER;
        }

        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS)
        {
            operation = sv::CameraOperation::ZOOM_UP;
        }
        if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS)
        {
            operation = sv::CameraOperation::ZOOM_DOWN;
        }

        if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS)
        {
            operation = sv::CameraOperation::ROTATE_FASTER;
        }
        if (glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS)
        {
            operation = sv::CameraOperation::ROTATE_SLOWER;
        }

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        {
            operation = sv::CameraOperation::FORWARD;
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        {
            operation = sv::CameraOperation::BACKWARD;
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        {
            operation = sv::CameraOperation::LEFT;
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        {
            operation = sv::CameraOperation::RIGHT;
        }
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        {
            operation = sv::CameraOperation::UP;
        }
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        {
            operation = sv::CameraOperation::DOWN;
        }

        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
        {
            operation = sv::CameraOperation::TURN_UP;
        }
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
        {
            operation = sv::CameraOperation::TURN_DOWN;
        }
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
        {
            operation = sv::CameraOperation::TURN_LEFT;
        }
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
        {
            operation = sv::CameraOperation::TURN_RIGHT;
        }

        static std::unique_ptr<sv::CameraOperation> operations;
        if (windowManager.GetWindowIndex() == 0 && !operations)
        {
            operations = std::unique_ptr<sv::CameraOperation>(new sv::CameraOperation[windowManager.GetWindowNum()]);
        }

        MPI_Gather(&operation, 1, MPI_INT, operations.get(), 1, MPI_INT, 0, MPI_COMM_WORLD);

        if (windowManager.GetWindowIndex() == 0)
        {
            for (int i = 0; i < windowManager.GetWindowNum(); i++)
            {
                camera->processCameraOperation(*(operations.get() + i), delta_time);
            }
        }
        MPI_Bcast(&camera->camera_pos, 26, MPI_FLOAT, 0, MPI_COMM_WORLD);
        MPI_Bcast(&camera->is_perspective, 1, MPI_C_BOOL, 0, MPI_COMM_WORLD);
    };
}