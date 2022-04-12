//
// Created by vr on 2021/4/15.
//

#ifndef VOLUMERENDERER_WINDOWMANAGER_H
#define VOLUMERENDERER_WINDOWMANAGER_H

#include <cmath>
#include <fstream>
#include <iostream>
#include <json.hpp>
#include <mpi.h>

using json = nlohmann::json;

class WindowManager
{
  public:
    static WindowManager &Instance()
    {
        static WindowManager _instance;
        return _instance;
    }

    void Init(int width, int height, int row, int col)
    {
        windowWidth = width;
        windowHeight = height;
        rowNum = row;
        colNum = col;

        MPI_Init(nullptr, nullptr);
        MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
        MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    }

    void Init(const std::string &configPath)
    {
        MPI_Init(nullptr, nullptr);
        MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
        MPI_Comm_size(MPI_COMM_WORLD, &world_size);

        std::ifstream in;
        in.open(configPath);
        assert(in.is_open());
        json config;
        in >> config;
        in.close();

        windowWidth = config["width"];
        windowHeight = config["height"];
        rowNum = config["row"];
        colNum = config["col"];
        nGPUHost = config["nGPUHost"];

        auto screenConfig = config["screen"][GetWindowIndex()];
        offsetX = screenConfig["offsetX"];
        offsetY = screenConfig["offsetY"];
        resourcePath = screenConfig["resourcePath"];
        auto blockNxJson = screenConfig["blockNx"];
        auto blockNyJson = screenConfig["blockNy"];
        if (blockNxJson.is_null())
        {
            isFixedBlockNum = false;
        }
        else
        {
            isFixedBlockNum = true;
            blockNx = blockNxJson;
            blockNy = blockNyJson;
        }
    }

    int GetWindowIndex() const
    {
        return world_rank;
    }

    int GetWindowNum() const
    {
        return world_size;
    }

    int GetWindowRowSize() const
    {
        return rowNum;
    }

    int GetWindowColSize() const
    {
        return colNum;
    }

    int GetTileWindowWidth() const
    {
        return windowWidth / GetWindowColSize();
    }

    int GetTileWindowHeight() const
    {
        return windowHeight / GetWindowRowSize();
    }

    int GetWholeWindowWidth() const
    {
        return windowWidth;
    }

    int GetWholeWindowHeight() const
    {
        return windowHeight;
    }

    int GetTileWindowOffsetX() const
    {
        return GetWindowIndex() % GetWindowColSize();
    };

    int GetTileWindowOffsetY() const
    {
        return GetWindowIndex() / GetWindowColSize();
    };

    std::string GetResourcePath() const
    {
        return resourcePath;
    }

    int GetWindowLocationOffsetX() const
    {
        return offsetX;
    }

    int GetWindowLocationOffsetY() const
    {
        return offsetY;
    }

    bool IsFixedBlockNum() const
    {
        return isFixedBlockNum;
    }

    int GetBlockNumX() const
    {
        return blockNx;
    }

    int GetBlockNumY() const
    {
        return blockNy;
    }
    int GetGPUNumPerHost() const
    {
        return nGPUHost;
    }

  private:
    int windowWidth = 400, windowHeight = 400;
    int nGPUHost = 1;
    int rowNum = 1, colNum = 1;
    int world_rank = 0, world_size = 1;
    int offsetX = 0, offsetY = 0;
    bool isFixedBlockNum = false;
    int blockNx = 3, blockNy = 3;
    std::string resourcePath;
    WindowManager() = default;
};

#endif // VOLUMERENDERER_WINDOWMANAGER_H
