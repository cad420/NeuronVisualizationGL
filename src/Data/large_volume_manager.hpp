//
// Created by wyz on 2021/4/13.
//

#ifndef NEURONVISUALIZATIONGL_LARGE_VOLUME_MANAGER_H
#define NEURONVISUALIZATIONGL_LARGE_VOLUME_MANAGER_H
#include<Common/utils.hpp>
#include<Common/help_cuda.hpp>
#include<unordered_map>
#include<VoxelCompression/voxel_uncompress/VoxelUncompress.h>
#include<VoxelCompression/voxel_compress/VoxelCmpDS.h>
#include<list>

#define WORKER_NUM 3
class Worker;

class CUMemPool;

class BlockDesc{
public:
    BlockDesc()=default;
//    BlockDesc(const BlockDesc&)=delete;
    BlockDesc(const std::array<uint32_t ,4>& idx):block_index(idx),data_ptr(0),size(0){}
    BlockDesc& operator=(const BlockDesc& block){
        this->block_index=block.block_index;
        this->data_ptr=block.data_ptr;
        this->size=block.size;
        return *this;
    }
    void release(){
        data_ptr=0;
        size=0;
    }
    std::array<uint32_t,4> block_index;
    CUdeviceptr data_ptr;
    int64_t size;
};

class BlockReqInfo{
public:
    //uncompress and load new blocks
    std::vector<std::array<uint32_t,4>> request_blocks_queue;
    //stop task for no need blocks
    std::vector<std::array<uint32_t,4>> noneed_blocks_queue;
};

class LargeVolumeReader;
class LargeVolumeManager{
public:
    explicit LargeVolumeManager(const char* lod_config_file);
    explicit LargeVolumeManager(const std::unordered_map<int,std::string>&);
    LargeVolumeManager(const LargeVolumeManager&)=delete;
    LargeVolumeManager(LargeVolumeManager&&)=delete;
    ~LargeVolumeManager();

public:
    auto getVolumeInfo()->std::tuple<uint32_t,uint32_t,uint32_t,uint32_t,std::array<uint32_t,3>>  ;

    void setupBlockReqInfo(const BlockReqInfo&);

    bool getBlock(BlockDesc&);

    void updateCUMemPool(CUdeviceptr);
private:
    void initResource();

    void initCUResource();

    void initUtilResource();

    void startWorking();
private:
    std::unique_ptr<LargeVolumeReader> volume_reader;
    uint32_t block_length;
    uint32_t padding;
    std::array<uint32_t,3> block_dim;
    int min_lod,max_lod;

    CUcontext cu_context;
    std::unique_ptr<CUMemPool> cu_mem_pool;
    std::vector<Worker> workers;
    std::list<BlockDesc> packages;
    std::unique_ptr<ThreadPool> jobs;
    ConcurrentQueue<BlockDesc> products;
    std::thread working_line;
    bool stop;
    std::mutex mtx;
    std::condition_variable worker_cv;
    std::condition_variable package_cv;
};
#endif //NEURONVISUALIZATIONGL_LARGE_VOLUME_MANAGER_H
