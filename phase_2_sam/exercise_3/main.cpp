#include <CL/cl.h>
#include <vector>
#include <iostream>
#include <fstream>
#include "lodepng.h"

#define CHECK(x) if(x!=CL_SUCCESS){printf("OpenCL error %d\n",x);exit(1);}

std::string loadKernel(const char* name)
{
    std::ifstream file(name);
    return std::string((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
}

double eventTime(cl_event event)
{
    cl_ulong start,end;
    clGetEventProfilingInfo(event,CL_PROFILING_COMMAND_START,sizeof(start),&start,NULL);
    clGetEventProfilingInfo(event,CL_PROFILING_COMMAND_END,sizeof(end),&end,NULL);
    return (end-start)/1e6;
}

int main()
{

    // ---------- Read image ----------

    std::vector<unsigned char> image;
    unsigned width,height;

    unsigned error = lodepng::decode(image,width,height,"im0.png");
    if(error) {
        std::cout<<"PNG decode error\n";
        return 1;
    }

    int newWidth = width/4;
    int newHeight = height/4;

    // ---------- OpenCL init ----------

    cl_platform_id platform;
    cl_device_id device;
    cl_context context;
    cl_command_queue queue;

    clGetPlatformIDs(1,&platform,NULL);
    clGetDeviceIDs(platform,CL_DEVICE_TYPE_GPU,1,&device,NULL);

    context = clCreateContext(NULL,1,&device,NULL,NULL,NULL);
    queue = clCreateCommandQueue(context,device,CL_QUEUE_PROFILING_ENABLE,NULL);

    // ---------- Load kernels ----------

    std::string source = loadKernel("kernels.cl");
    const char* src = source.c_str();

    cl_program program = clCreateProgramWithSource(context,1,&src,NULL,NULL);
    clBuildProgram(program,0,NULL,NULL,NULL,NULL);

    cl_kernel resizeKernel = clCreateKernel(program,"resizeImage",NULL);
    cl_kernel grayKernel = clCreateKernel(program,"grayscale",NULL);
    cl_kernel filterKernel = clCreateKernel(program,"filter5x5",NULL);


}