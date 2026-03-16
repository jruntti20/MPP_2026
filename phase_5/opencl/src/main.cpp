#include <iostream>
#include <fstream>
#include <vector>
#include <CL/cl.h>
#include "lodepng.h"

#define WIDTH 2940
#define HEIGHT 2016
#define CHANNELS 4
#define MAX_DISPARITY 64
#define WINDOW 5

typedef unsigned char BYTE;

std::string loadKernelSource(const char* filename)
{
    std::ifstream file(filename);
    if(!file.is_open())
    {
        std::cerr << "Failed to open kernel file: " << filename << std::endl;
        exit(1);
    }
    return std::string(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    );
}

int main()
{
    const char imgPathLeft[] = "../img/im0.png";
    const char imgPathRight[] = "../img/im1.png";

    unsigned int w = WIDTH;
    unsigned int h = HEIGHT;

    unsigned char* inputImageLeft = NULL;
    unsigned char* inputImageRight = NULL;

    unsigned error;

    error = lodepng_decode_file(&inputImageLeft, &w, &h, imgPathLeft, LCT_RGBA, 8);
    if(error) { std::cout << "Error loading left image\n"; return -1; }

    error = lodepng_decode_file(&inputImageRight, &w, &h, imgPathRight, LCT_RGBA, 8);
    if(error) { std::cout << "Error loading right image\n"; return -1; }

    std::cout << "Images loaded: " << w << " x " << h << std::endl;

    /* ---------- OPENCL SETUP ---------- */

    cl_int err;
    cl_uint num_platforms;
    clGetPlatformIDs(0, NULL, &num_platforms);
    std::vector<cl_platform_id> platforms(num_platforms);
    clGetPlatformIDs(num_platforms, platforms.data(), NULL);

    cl_platform_id platform = NULL;
    for(cl_uint i=0;i<num_platforms;i++)
    {
        char name[128];
        clGetPlatformInfo(platforms[i],CL_PLATFORM_NAME,sizeof(name),name,NULL);
        std::cout<<"Found platform: "<<name<<std::endl;
        if(std::string(name).find("NVIDIA")!=std::string::npos)
            platform = platforms[i];
    }
    if(platform==NULL) { std::cout<<"No NVIDIA platform found\n"; return -1; }

    cl_device_id device;
    err = clGetDeviceIDs(platform,CL_DEVICE_TYPE_GPU,1,&device,NULL);
    if(err!=CL_SUCCESS){ std::cout<<"Failed to get GPU device\n"; return -1; }
    std::cout<<"GPU device acquired\n";

    char device_name[128];
    clGetDeviceInfo(device,CL_DEVICE_NAME,sizeof(device_name),device_name,NULL);
    std::cout<<"Device: "<<device_name<<std::endl;

    cl_context context = clCreateContext(NULL,1,&device,NULL,NULL,&err);
    cl_command_queue queue = clCreateCommandQueue(context,device,0,&err);

    /* ---------- BUFFERS ---------- */

    size_t imageSize = w*h*CHANNELS;
    cl_mem leftBuffer = clCreateBuffer(context,CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR,imageSize,inputImageLeft,&err);
    cl_mem rightBuffer = clCreateBuffer(context,CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR,imageSize,inputImageRight,&err);
    std::vector<unsigned char> disparity(w*h);
    cl_mem disparityBuffer = clCreateBuffer(context,CL_MEM_WRITE_ONLY,w*h*sizeof(unsigned char),NULL,&err);

    size_t global[2] = {w,h};

    /* ---------- KERNEL 1 FAST ---------- */

    std::string kernelSource1 = loadKernelSource("kernel_fast.cl");
    const char* src1 = kernelSource1.c_str();
    size_t src1size = kernelSource1.length();

    cl_program program1 = clCreateProgramWithSource(context,1,&src1,&src1size,&err);
    err = clBuildProgram(program1,1,&device,NULL,NULL,NULL);
    if(err!=CL_SUCCESS)
    {
        size_t log_size;
        clGetProgramBuildInfo(program1,device,CL_PROGRAM_BUILD_LOG,0,NULL,&log_size);
        std::vector<char> log(log_size);
        clGetProgramBuildInfo(program1,device,CL_PROGRAM_BUILD_LOG,log_size,log.data(),NULL);
        std::cout<<log.data()<<std::endl;
        return -1;
    }

    cl_kernel kernel1 = clCreateKernel(program1,"zncc_fast",&err);
    clSetKernelArg(kernel1,0,sizeof(cl_mem),&leftBuffer);
    clSetKernelArg(kernel1,1,sizeof(cl_mem),&rightBuffer);
    clSetKernelArg(kernel1,2,sizeof(cl_mem),&disparityBuffer);
    clSetKernelArg(kernel1,3,sizeof(int),&w);
    clSetKernelArg(kernel1,4,sizeof(int),&h);

    clEnqueueNDRangeKernel(queue,kernel1,2,NULL,global,NULL,0,NULL,NULL);
    clFinish(queue);

    clEnqueueReadBuffer(queue,disparityBuffer,CL_TRUE,0,w*h*sizeof(unsigned char),disparity.data(),0,NULL,NULL);

    std::vector<unsigned char> output1(w*h*4);
    for(int i=0;i<w*h;i++)
    {
        unsigned char d=disparity[i];
        output1[i*4+0]=d;
        output1[i*4+1]=d;
        output1[i*4+2]=d;
        output1[i*4+3]=255;
    }
    lodepng_encode32_file("../Output_images/depth1.png",output1.data(),w,h);
    std::cout<<"Depth map 1 saved\n";

    /* ---------- KERNEL 2 CPU STYLE ---------- */

    std::string kernelSource2 = loadKernelSource("kernel_cpu.cl");
    const char* src2 = kernelSource2.c_str();
    size_t src2size = kernelSource2.length();

    cl_program program2 = clCreateProgramWithSource(context,1,&src2,&src2size,&err);
    err = clBuildProgram(program2,1,&device,NULL,NULL,NULL);
    if(err!=CL_SUCCESS)
    {
        size_t log_size;
        clGetProgramBuildInfo(program2,device,CL_PROGRAM_BUILD_LOG,0,NULL,&log_size);
        std::vector<char> log(log_size);
        clGetProgramBuildInfo(program2,device,CL_PROGRAM_BUILD_LOG,log_size,log.data(),NULL);
        std::cout<<log.data()<<std::endl;
        return -1;
    }

    cl_kernel kernel2 = clCreateKernel(program2,"zncc_cpu",&err);

    int win_size = WINDOW*2+1;
    int min_d = 0;
    int max_d = MAX_DISPARITY;

    clSetKernelArg(kernel2,0,sizeof(cl_mem),&leftBuffer);
    clSetKernelArg(kernel2,1,sizeof(cl_mem),&rightBuffer);
    clSetKernelArg(kernel2,2,sizeof(cl_mem),&disparityBuffer);
    clSetKernelArg(kernel2,3,sizeof(int),&w);
    clSetKernelArg(kernel2,4,sizeof(int),&h);
    clSetKernelArg(kernel2,5,sizeof(int),&win_size);
    clSetKernelArg(kernel2,6,sizeof(int),&min_d);
    clSetKernelArg(kernel2,7,sizeof(int),&max_d);

    clEnqueueNDRangeKernel(queue,kernel2,2,NULL,global,NULL,0,NULL,NULL);
    clFinish(queue);

    clEnqueueReadBuffer(queue,disparityBuffer,CL_TRUE,0,w*h*sizeof(unsigned char),disparity.data(),0,NULL,NULL);

    std::vector<unsigned char> output2(w*h*4);
    for(int i=0;i<w*h;i++)
    {
        unsigned char d=disparity[i];
        output2[i*4+0]=d;
        output2[i*4+1]=d;
        output2[i*4+2]=d;
        output2[i*4+3]=255;
    }
    lodepng_encode32_file("../Output_images/depth2.png",output2.data(),w,h);
    std::cout<<"Depth map 2 saved\n";

    /* ---------- CLEANUP ---------- */
    clReleaseKernel(kernel1);
    clReleaseProgram(program1);
    clReleaseKernel(kernel2);
    clReleaseProgram(program2);
    clReleaseMemObject(leftBuffer);
    clReleaseMemObject(rightBuffer);
    clReleaseMemObject(disparityBuffer);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);

    free(inputImageLeft);
    free(inputImageRight);

    return 0;
}
