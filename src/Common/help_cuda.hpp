//
// Created by wyz on 2021/2/26.
//

#ifndef VOLUMERENDERER_HELP_CUDA_H
#define VOLUMERENDERER_HELP_CUDA_H
#include <cuda.h>
#include <cuda_runtime.h>
#include <spdlog/spdlog.h>
inline bool check(CUresult e, int iLine, const char *szFile)
{
    if (e != CUDA_SUCCESS)
    {
        const char *szErrName = NULL;
        cuGetErrorName(e, &szErrName);
        spdlog::error("CUDA driver API error: {0} at line {1:d} in file {2}", szErrName, iLine, szFile);
        return false;
    }
    return true;
}
inline bool check(cudaError_t e, int line, const char *file)
{
    if (e != cudaSuccess)
    {
        const char *error_name = nullptr;
        error_name = cudaGetErrorName(e);
        spdlog::error("CUDA runtime API error: {0} at line {1:d} in file {2}", error_name, line, file);
        return false;
    }
    return true;
}

#define CUDA_DRIVER_API_CALL(call) check(call, __LINE__, __FILE__)

#define CUDA_RUNTIME_API_CALL(call) check(call, __LINE__, __FILE__)

#endif // VOLUMERENDERER_HELP_CUDA_H
