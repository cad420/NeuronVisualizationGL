//
// Created by wyz on 2021/4/13.
//
#include"large_volume_manager.hpp"
#include<IO/large_volume_reader.hpp>
#include <iostream>
class Worker{
public:
    Worker(const VoxelUncompressOptions& opt){
        uncmp=std::make_unique<VoxelUncompress>(opt);
        status._a=false;
    }
    bool isBusy() const{
        return status._a;
    }
    void setStatus(bool _status){
        status._a=_status;
    }
    void uncompress(uint8_t* dest_ptr,int64_t len,std::vector<std::vector<uint8_t>>& packets){
        uncmp->uncompress(dest_ptr,len,packets);
    }
private:
    std::unique_ptr<VoxelUncompress> uncmp;
    atomic_wrapper<bool> status;
};
class CUMem{
public:
    CUMem():data_ptr(0){
        status._a=false;
    }
    CUdeviceptr data_ptr;
    atomic_wrapper<bool> status;
};
class CUMemPool{
public:
    CUMemPool()=default;
    void setBlockSizeBytes(int64_t size){
        this->block_size_bytes=size;
    }
    int64_t getBlockSizeBytes() const{
        return this->block_size_bytes;
    }
    void addCUMem(CUMem&& m){
        mems.emplace_back(m);
        num++;
    }
    int getCUMemSize() const{
        return mems.size();
    }
    CUMem& getCUMem(){
        std::unique_lock<std::mutex> lk(mtx);
        cv.wait(lk,[&](){
            for(auto&mem:mems){
                if(!mem.status._a){

                    return true;
                }
            }
            std::cout<<"no cu_mem in cu_mem_pool !!!"<<std::endl;
            return false;
        });
        for(auto&mem: mems){
            if(!mem.status._a){
                mem.status._a=true;
                num--;
                std::cout<<"get cu_mem: "<<num<<std::endl;
                cv.notify_one();
                return mem;
            }
        }
        throw std::runtime_error("get cu_mem reach final");
    }
    void returnCUMem(CUdeviceptr cu_mem){
        for(auto& mem:mems){
            if(mem.data_ptr==cu_mem){
                mem.status._a=false;
                num++;
                std::cout<<"return cu_mem: "<<num<<std::endl;
                cv.notify_one();
                return;
            }
        }
        throw std::runtime_error("return cu_mem reach final");
    }
private:
    int num=0;
    int64_t block_size_bytes;
    std::vector<CUMem> mems;
    std::condition_variable cv;
    std::mutex mtx;
};
LargeVolumeManager::LargeVolumeManager(const char *lod_config_file)
:stop(false)
{
    this->volume_reader=std::make_unique<LargeVolumeReader>(lod_config_file);
    initResource();
    startWorking();
}

LargeVolumeManager::LargeVolumeManager(const std::unordered_map<int, std::string> &lods)
:stop(false)
{
    this->volume_reader=std::make_unique<LargeVolumeReader>(lods);
    initResource();
    startWorking();
}

LargeVolumeManager::~LargeVolumeManager() {
    this->stop=true;
    package_cv.notify_all();
    jobs.reset(nullptr);
    if(this->working_line.joinable())
        this->working_line.join();
//    CUDA_DRIVER_API_CALL(cuCtxDestroy(cu_context));
}

auto LargeVolumeManager::getVolumeInfo() -> std::tuple<uint32_t, uint32_t, uint32_t, uint32_t, std::array<uint32_t, 3>> {

    return std::make_tuple(block_length,padding,min_lod,max_lod,block_dim);

}

void LargeVolumeManager::initResource() {
    initUtilResource();
    initCUResource();
}

void LargeVolumeManager::initCUResource() {
    CUDA_DRIVER_API_CALL(cuInit(0));
    CUdevice cu_device;
    CUDA_DRIVER_API_CALL(cuDeviceGet(&cu_device,0));
    cuCtxCreate(&cu_context,0,cu_device);

    assert(block_length);
    uint64_t block_byte_size=(uint64_t)block_length*block_length*block_length;
    int cu_mem_size=8;
    cu_mem_pool=std::make_unique<CUMemPool>();
    cu_mem_pool->setBlockSizeBytes(block_byte_size);
    for(int i=0;i<cu_mem_size;i++){
        CUMem cu_mem;
        CUDA_DRIVER_API_CALL(cuMemAlloc(&cu_mem.data_ptr,block_byte_size));
        cu_mem_pool->addCUMem(std::move(cu_mem));
    }
}

void LargeVolumeManager::initUtilResource() {
    auto header_info=this->volume_reader->getHeaderInfo();

    this->block_length=std::pow(2,header_info.log_block_length);
    this->padding=header_info.padding;
    this->block_dim={header_info.block_dim_x,header_info.block_dim_y,header_info.block_dim_z};
    volume_reader->getLodInfo(this->min_lod,this->max_lod);

    VoxelUncompressOptions uncmp_opts;
    uncmp_opts.width=header_info.frame_width;
    uncmp_opts.height=header_info.frame_height;
    uncmp_opts.use_device_frame_buffer=true;
    for(int i=0;i<WORKER_NUM;i++){
        workers.emplace_back(uncmp_opts);
    }

    jobs=std::make_unique<ThreadPool>(WORKER_NUM);

    products.setSize(WORKER_NUM*2);

}

void LargeVolumeManager::startWorking() {
    this->working_line=std::thread([&]()->void{
        while(true){
            if(this->stop)
                return;

            //1.find worker
            {
                std::unique_lock<std::mutex> lk(mtx);
                worker_cv.wait(lk,[&](){
                    for(auto& worker:workers){
                        if(!worker.isBusy() || stop){
                            return true;
                        }
                    }
//                   std::cout<<"all workers are busy!!!"<<std::endl;
                    return false;
                });
                if(stop)
                    return;
            }

            //2.find package
            for(int i=0;i<workers.size();i++){
                auto& worker=workers[i];
                if(!worker.isBusy()){
                    worker.setStatus(true);
                    BlockDesc package;
                    {
                        std::unique_lock<std::mutex> lk(mtx);

                        package_cv.wait(lk,[&](){

                            if(packages.empty() && !stop){
//                               std::cout<<"packages empty!!!"<<std::endl;
                                return false;
                            }
                            else
                                return true;
                        });
                        if(stop)
                            return;
                        package=packages.front();
                        packages.pop_front();
                    }
                    jobs->AppendTask([&](int idx,BlockDesc block_desc){
//                        std::cout<<"start job"<<std::endl;
                        std::vector<std::vector<uint8_t>> packet;
//                        spdlog::info("get packet");

                        volume_reader->read_packet(block_desc.block_index[3],{block_desc.block_index[0],block_desc.block_index[1],block_desc.block_index[2]},packet);

                        //                        spdlog::info("get cuda mem");
//                        std::cout<<products.size()<<std::endl;
                        auto& cu_mem=cu_mem_pool->getCUMem();
//                        spdlog::info("start uncompress");
                        START_CPU_TIMER
                        workers[idx].uncompress((uint8_t*)cu_mem.data_ptr,cu_mem_pool->getBlockSizeBytes(),packet);
                        END_CPU_TIMER
//                        spdlog::info("finish uncompress");
                        block_desc.data_ptr=cu_mem.data_ptr;
                        block_desc.size=cu_mem_pool->getBlockSizeBytes();

                        products.push_back(block_desc);

                        workers[idx].setStatus(false);

                        worker_cv.notify_one();

                        std::cout<<"finish job"<<std::endl;
                    },i,package);
                }
            }

        }
    });
}

void LargeVolumeManager::setupBlockReqInfo(const BlockReqInfo &request) {
    {
        std::unique_lock<std::mutex> lk(mtx);
        for (auto &idx:request.noneed_blocks_queue) {
            for (auto it = packages.cbegin(); it != packages.cend(); it++) {
                if (it->block_index == idx) {
                    packages.erase(it);
                    break;
                }
            }
        }
        packages.insert(packages.cend(), request.request_blocks_queue.cbegin(), request.request_blocks_queue.cend());
    }
    package_cv.notify_one();
}

bool LargeVolumeManager::getBlock(BlockDesc &block) {
    if(products.empty()){
        return false;
    }
    else{
        block=products.pop_front();
        return true;
    }
}

void LargeVolumeManager::updateCUMemPool(CUdeviceptr cu_mem) {
    this->cu_mem_pool->returnCUMem(cu_mem);
}
