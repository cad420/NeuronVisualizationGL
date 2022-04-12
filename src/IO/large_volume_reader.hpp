//
// Created by wyz on 2021/4/13.
//

#ifndef NEURONVISUALIZATIONGL_LARGE_VOLUME_READER_H
#define NEURONVISUALIZATIONGL_LARGE_VOLUME_READER_H

#include <VoxelCompression/voxel_compress/VoxelCmpDS.h>
#include <unordered_map>
class LargeVolumeReader
{
  public:
    explicit LargeVolumeReader(const char *lod_config_file);
    explicit LargeVolumeReader(const std::unordered_map<int, std::string> &);
    void read_packet(int lod, const std::array<uint32_t, 3> &index, std::vector<std::vector<uint8_t>> &packet);
    sv::Header getHeaderInfo();
    void getLodInfo(int &min_lod, int &max_lod);

  private:
    int min_lod, max_lod;
    std::unordered_map<int, std::unique_ptr<sv::Reader>> readers;
};

#endif // NEURONVISUALIZATIONGL_LARGE_VOLUME_READER_H
