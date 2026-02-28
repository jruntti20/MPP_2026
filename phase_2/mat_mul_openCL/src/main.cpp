#include <CL/cl.h>
#include <stdlib.h>
#include <iostream>

#include "mat_mul.h"
#define CL_HPP_TARGET_OPENCL_VERSION 300

int main(int argc, char* argv[])
{

    cl_platform_id *platforms = NULL;
    cl_uint numPlatforms = 0;
    cl_uint maxComputeUnits;
    cl_uint maxWorkItemDimensions;
    cl_uint maxWorkGroupSize;

    cl_uint numDevices;
    cl_device_id deviceIDs[1];
    
    char queryBuffer[1024] = {};
    cl_int clError;
    size_t size;
    size_t size_t_buff[64] = {};

    clError = clGetPlatformIDs(0, NULL, &numPlatforms);
    platforms = (cl_platform_id *)malloc(numPlatforms*sizeof(cl_platform_id));
    clError = clGetPlatformIDs(numPlatforms, platforms, NULL);
    if (clError != CL_SUCCESS || numPlatforms <= 0)
    {
        std::cerr << "Failed to find any OpenCL plarform." << std::endl;
        exit(1);
    }

    for(cl_uint i = 0; i < numPlatforms; i++)
    {
        // Print out some platform info
        clGetPlatformInfo(platforms[i], CL_PLATFORM_NAME, 1024, &queryBuffer, NULL);
        std::cout << "CL_PLATFORM_NAME: " << queryBuffer << std::endl;
        clGetPlatformInfo(platforms[i], CL_PLATFORM_VENDOR, 1024, &queryBuffer, NULL);
        std::cout << "CL_PLATFORM_VENDOR: " << queryBuffer << std::endl;
        clGetPlatformInfo(platforms[i], CL_PLATFORM_VERSION, 1024, &queryBuffer, NULL);
        std::cout << "CL_PLATFORM_VERSION: " << queryBuffer << std::endl;
        clGetPlatformInfo(platforms[i], CL_PLATFORM_PROFILE, 1024, &queryBuffer, NULL);
        std::cout << "CL_PLATFORM_PROFILE: " << queryBuffer << std::endl;
        clGetPlatformInfo(platforms[i], CL_PLATFORM_EXTENSIONS, 1024, &queryBuffer, NULL);
        std::cout << "CL_PLATFORM_EXTENSIONS: " << queryBuffer << std::endl;

        // Print out some device info
        clError = clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_GPU, 0, NULL, &numDevices);

        if (numDevices < 1)
        {
            std::cout << "No GPU device found from platform " << platforms[i] << std::endl;
            exit(1);
        }
        clError = clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_GPU, 1, &deviceIDs[0], NULL);

        clError = clGetDeviceInfo(deviceIDs[0], CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(cl_uint), &maxComputeUnits, &size);
        std::cout << "Device has max compute units: " << maxComputeUnits << std::endl;
        clError = clGetDeviceInfo(deviceIDs[0], CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS, sizeof(cl_uint), &maxWorkItemDimensions, &size);
        std::cout << "Device has max work item dimensions: " << maxWorkItemDimensions << std::endl;
        clError = clGetDeviceInfo(deviceIDs[0], CL_DEVICE_MAX_WORK_ITEM_SIZES, sizeof(size_t_buff), size_t_buff, &size);
        std::cout << "Device has max work item sizes: (" << size_t_buff[0] << "," << size_t_buff[1] << "," <<  size_t_buff[2] << ")" << std::endl;
        clError = clGetDeviceInfo(deviceIDs[0], CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(size_t), &maxWorkGroupSize, &size);
        std::cout << "Device has max work group size: " << maxWorkGroupSize << std::endl;

    }





    return 0;
}
