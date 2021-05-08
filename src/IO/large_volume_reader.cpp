//
// Created by wyz on 2021/4/13.
//
#include"large_volume_reader.hpp"
#include <spdlog/spdlog.h>
LargeVolumeReader::LargeVolumeReader(const char *lod_config_file) {
    sv::LodFile lod_file;
    lod_file.open_lod_file(lod_config_file);
    this->min_lod=lod_file.get_min_lod();
    this->max_lod=lod_file.get_max_lod();
    for(int i=min_lod;i<=max_lod;i++){
        readers[i]=std::make_unique<sv::Reader>(lod_file.get_lod_file_path(i).c_str());
        readers[i]->read_header();
    }
    if(this->min_lod<0 || this->max_lod>10){
        throw std::runtime_error("error min_lod or max_lod");
    }
    spdlog::info("Load lod volume file, mid_lod: {0:d} max_lod: {1:d}",min_lod,max_lod);
}

LargeVolumeReader::LargeVolumeReader(const std::unordered_map<int, std::string> &lods) {
    this->min_lod=1024;
    this->max_lod=-1;
    for(auto& it:lods){
        if(it.first<this->min_lod)
            this->min_lod=it.first;
        if(it.first>this->max_lod)
            this->max_lod=it.first;
        readers[it.first]=std::make_unique<sv::Reader>(it.second.c_str());
        readers[it.first]->read_header();
    }
    if(this->min_lod<0 || this->max_lod>10){
        throw std::runtime_error("error min_lod or max_lod");
    }
}

void LargeVolumeReader::read_packet(int lod, const std::array<uint32_t, 3> &index,std::vector<std::vector<uint8_t>> &packet)
{
    assert(lod>=min_lod && lod<=max_lod);
    readers[lod]->read_packet(index,packet);
}

sv::Header LargeVolumeReader::getHeaderInfo() {
    return readers[min_lod]->get_header();
}

void LargeVolumeReader::getLodInfo(int &min_lod, int &max_lod) {
    min_lod=this->min_lod;
    max_lod=this->max_lod;
}
