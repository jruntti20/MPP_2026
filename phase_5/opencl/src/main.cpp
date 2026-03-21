#include <iostream>
#include <fstream>
#include <vector>
#include <CL/cl.h>
#include "lodepng.h"
#include <sys/time.h>

#define WIDTH 2940
#define HEIGHT 2016
#define WINDOW 13
#define CHANNELS 4
#define MAX_DISPARITY 64
#define RESIZE_FACTOR 4

typedef unsigned char BYTE;

struct timeval t[6];
struct timeval pixeltime[2];

std::string loadKernelSource(const char* filename)
{
    std::string src_path("../src/");
    src_path.append(filename);
    std::ifstream file(src_path);
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
    unsigned int d = MAX_DISPARITY;
    int window = WINDOW;
    unsigned int resize_factor = RESIZE_FACTOR;
    unsigned int resized_w = w / resize_factor;
    unsigned int resized_h = h / resize_factor;

    //unsigned char* inputImageLeft = NULL;
    //unsigned char* inputImageRight = NULL;

    unsigned char* inputImageLeft = (unsigned char*)malloc(w * h * 4 * sizeof(unsigned char));
    unsigned char* inputImageRight = (unsigned char*)malloc(w * h * 4 * sizeof(unsigned char));
    unsigned char* tinyGrayLeft = (unsigned char *)malloc(w/resize_factor * h/resize_factor * sizeof(unsigned char));
    unsigned char* tinyGrayRight = (unsigned char *)malloc(w/resize_factor * h/resize_factor * sizeof(unsigned char));
    unsigned char* disparityL = (unsigned char *)calloc(w / resize_factor * h / resize_factor, sizeof(unsigned char));
    unsigned char* disparityR = (unsigned char *)calloc(w / resize_factor * h / resize_factor, sizeof(unsigned char));
    unsigned char *crosscheckedL = (unsigned char *)calloc(w/resize_factor * h/resize_factor, sizeof(unsigned char));
    unsigned char *occlusionL = (unsigned char *)calloc(w/resize_factor * h/resize_factor, sizeof(unsigned char));

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

    size_t imageSize = w*h*CHANNELS * sizeof(unsigned char);
    size_t tinyGrayImageSize = w/4 * h/4 * sizeof(unsigned char);

    cl_mem leftBuffer = clCreateBuffer(context,CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR,imageSize,inputImageLeft,&err);
    cl_mem rightBuffer = clCreateBuffer(context,CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR,imageSize,inputImageRight,&err);
    cl_mem tinyGrayBufferL = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, tinyGrayImageSize, tinyGrayLeft, &err);
    cl_mem tinyGrayBufferR = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, tinyGrayImageSize, tinyGrayRight, &err);

    cl_mem disparityBufferL = clCreateBuffer(context,CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,tinyGrayImageSize,disparityL,&err);
    cl_mem disparityBufferR = clCreateBuffer(context,CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,tinyGrayImageSize,disparityR,&err);
    cl_mem crosscheckedBufL = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, tinyGrayImageSize, crosscheckedL, &err); 
    cl_mem occlusionBufferL = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, tinyGrayImageSize, occlusionL, &err); 

    // Load kernel functions
    std::string kernelSource1 = loadKernelSource("kernels.cl");
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
    /* ---------- KERNEL 1 RESIZE AND GRAYSCALE ---------- */

    gettimeofday(&t[0], NULL);
    size_t global1[2] = {size_t(resized_h), size_t(resized_w)};

    cl_kernel kernel1 = clCreateKernel(program1,"resize_grayscale_image",&err);
    clSetKernelArg(kernel1,0,sizeof(cl_mem),&leftBuffer);
    clSetKernelArg(kernel1,1,sizeof(cl_mem),&rightBuffer);
    clSetKernelArg(kernel1,2,sizeof(cl_mem),&tinyGrayBufferL);
    clSetKernelArg(kernel1,3,sizeof(cl_mem),&tinyGrayBufferR);
    clSetKernelArg(kernel1,4,sizeof(cl_uchar),&resize_factor);
    clSetKernelArg(kernel1,5,sizeof(cl_uint),&w);

    clEnqueueNDRangeKernel(queue,kernel1,2,NULL,global1,NULL,0,NULL,NULL);
    clFinish(queue);

    clEnqueueReadBuffer(queue,tinyGrayBufferL,CL_TRUE,0,w/resize_factor*h/resize_factor*sizeof(unsigned char),tinyGrayLeft,0,NULL,NULL);
    clEnqueueReadBuffer(queue,tinyGrayBufferR,CL_TRUE,0,w/resize_factor*h/resize_factor*sizeof(unsigned char),tinyGrayRight,0,NULL,NULL);

    lodepng_encode_file("../Output_images/tinyGrayLeft.png", tinyGrayLeft, w/resize_factor, h/resize_factor, LCT_GREY, 8); 
    lodepng_encode_file("../Output_images/tinyGrayRight.png", tinyGrayRight, w/resize_factor, h/resize_factor, LCT_GREY, 8); 


    gettimeofday(&t[1], NULL);
    int zero = 0;
    cl_mem debugBuf = clCreateBuffer(context, CL_MEM_WRITE_ONLY, 8192, NULL, &err);
    cl_mem debugIndex = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(int), NULL, &err);
    clEnqueueWriteBuffer(queue, debugIndex, CL_TRUE, 0, sizeof(int), &zero, 0, NULL, NULL);
    char buffer[8192];

    /*-------- DEBUG KERNEL TESTING --------*/
    //cl_kernel testKernel = clCreateKernel(program1, "testKernel", &err);
    //clSetKernelArg(testKernel, 0, sizeof(cl_mem), &debugBuf);
    //clSetKernelArg(testKernel, 1, sizeof(cl_mem), &debugIndex);
    //size_t testGlobalSize[2] = {16,16};
    //clEnqueueNDRangeKernel(queue, testKernel, 2, NULL, testGlobalSize, NULL, 0, NULL, NULL); 
    //clFinish(queue);
    //clEnqueueReadBuffer(queue, debugBuf, CL_TRUE, 0, 8192, buffer, 0, NULL, NULL);
    //printf("%s\n", buffer);

    /* ---------- KERNEL 1 FAST ---------- */
    //size_t global2[3] = {size_t(resized_w),size_t(resized_h),size_t(d)};
    size_t global2[2] = {size_t(resized_w),size_t(resized_h)};
    //size_t global2[2] = {((resized_w + 15) / 16) * 16,((resized_h + 15) / 16) * 16};
    //size_t local2[2] = {16, 16};
    int disp_sign = 1;

    //std::string kernelSource1 = loadKernelSource("kernel_fast.cl");

    clEnqueueWriteBuffer(queue, debugIndex, CL_TRUE, 0, sizeof(int), &zero, 0, NULL, NULL);

    cl_kernel kernel2 = clCreateKernel(program1,"zncc_fast",&err);
    clSetKernelArg(kernel2,0,sizeof(cl_mem),&tinyGrayBufferL);
    clSetKernelArg(kernel2,1,sizeof(cl_mem),&tinyGrayBufferR);
    clSetKernelArg(kernel2,2,sizeof(cl_mem),&disparityBufferL);
    clSetKernelArg(kernel2,3,sizeof(cl_int),&disp_sign);
    clSetKernelArg(kernel2,4,sizeof(cl_int),&resized_w);
    clSetKernelArg(kernel2,5,sizeof(cl_int),&resized_h);
    clSetKernelArg(kernel2,6, sizeof(cl_int),&window); 
    clSetKernelArg(kernel2,7,sizeof(cl_mem),&debugBuf);
    clSetKernelArg(kernel2,8,sizeof(cl_mem),&debugIndex);


    clEnqueueNDRangeKernel(queue,kernel2,2,NULL,global2, NULL,0,NULL,NULL);
    clFinish(queue);

    clEnqueueReadBuffer(queue,disparityBufferL,CL_TRUE,0,resized_w
            *resized_h*sizeof(unsigned char),disparityL,0,NULL,NULL);

    clEnqueueReadBuffer(queue, debugBuf, CL_TRUE, 0, 8192, buffer, 0, NULL, NULL);
    printf("%s\n", buffer);

    /*
    std::vector<unsigned char> output1(w*h*4);
    for(int i=0;i<w*h;i++)
    {
        unsigned char d=disparityL[i];
        output1[i*4+0]=d;
        output1[i*4+1]=d;
        output1[i*4+2]=d;
        output1[i*4+3]=255;
    }
    */
    //lodepng_encode32_file("../Output_images/depth1.png",output1.data(),w,h);

    lodepng_encode_file("../Output_images/depth1.png", disparityL, resized_w, resized_h, LCT_GREY, 8);
    std::cout<<"Depth map 1 saved\n";

    disp_sign = -1;

    kernel2 = clCreateKernel(program1,"zncc_fast",&err);
    clSetKernelArg(kernel2,0,sizeof(cl_mem),&tinyGrayBufferR);
    clSetKernelArg(kernel2,1,sizeof(cl_mem),&tinyGrayBufferL);
    clSetKernelArg(kernel2,2,sizeof(cl_mem),&disparityBufferR);
    clSetKernelArg(kernel2,3,sizeof(cl_int),&disp_sign);
    clSetKernelArg(kernel2,4,sizeof(cl_int),&resized_w);
    clSetKernelArg(kernel2,5,sizeof(cl_int),&resized_h);
    clSetKernelArg(kernel2,6, sizeof(cl_int),&window); 
    clSetKernelArg(kernel2,7,sizeof(cl_mem),&debugBuf);
    clSetKernelArg(kernel2,8,sizeof(cl_mem),&debugIndex);

    clEnqueueNDRangeKernel(queue,kernel2,2,NULL,global2, NULL,0, NULL,NULL);
    clFinish(queue);

    clEnqueueReadBuffer(queue,disparityBufferR,CL_TRUE,0,resized_w*resized_h*sizeof(unsigned char),disparityR,0,NULL,NULL);
    

    /*
    std::vector<unsigned char> output2(w*h*4);
    for(int i=0;i<w*h;i++)
    {
        unsigned char d=disparityR[i];
        output2[i*4+0]=d;
        output2[i*4+1]=d;
        output2[i*4+2]=d;
        output2[i*4+3]=255;
    }
    */
    //lodepng_encode32_file("../Output_images/depth2.png",output2.data(),w,h);
    lodepng_encode_file("../Output_images/depth2.png", disparityR, resized_w, resized_h, LCT_GREY, 8);
    
    std::cout<<"Depth map 2 saved\n";
    gettimeofday(&t[2], NULL);

    int threshold = 20;
    
    clEnqueueWriteBuffer(queue, debugIndex, CL_TRUE, 0, sizeof(int), &zero, 0, NULL, NULL);


    cl_kernel kernel3 = clCreateKernel(program1, "cross_checking", &err);
    clSetKernelArg(kernel3, 0, sizeof(cl_mem), &disparityBufferL);
    clSetKernelArg(kernel3, 1, sizeof(cl_mem), &disparityBufferR);
    clSetKernelArg(kernel3, 2, sizeof(cl_mem), &crosscheckedBufL);
    clSetKernelArg(kernel3, 3, sizeof(cl_int), &resized_w);
    clSetKernelArg(kernel3, 4, sizeof(cl_int), &resized_h);
    clSetKernelArg(kernel3, 5, sizeof(cl_int), &threshold);
    clSetKernelArg(kernel3, 6, sizeof(cl_int), &window);
    clSetKernelArg(kernel3,7,sizeof(cl_mem),&debugBuf);
    clSetKernelArg(kernel3,8,sizeof(cl_mem),&debugIndex);

    clEnqueueNDRangeKernel(queue,kernel3,2,NULL,global2, NULL,0, NULL,NULL);
    clFinish(queue);

    clEnqueueReadBuffer(queue,crosscheckedBufL,CL_TRUE,0,resized_w*resized_h*sizeof(unsigned char),crosscheckedL,0,NULL,NULL);
    clEnqueueReadBuffer(queue, debugBuf, CL_TRUE, 0, 8192, buffer, 0, NULL, NULL);
    printf("%s\n", buffer);


    lodepng_encode_file("../Output_images/cross_checked.png", crosscheckedL, resized_w, resized_h, LCT_GREY, 8);
    printf("Cross checked output image saved.\n");


    gettimeofday(&t[3], NULL);

    int neighbourhood_range = 50;

    cl_kernel kernel4 = clCreateKernel(program1, "occlusion_filling", &err);
    clSetKernelArg(kernel4, 0, sizeof(cl_mem), &occlusionBufferL);
    clSetKernelArg(kernel4, 1, sizeof(cl_mem), &crosscheckedBufL);
    clSetKernelArg(kernel4, 2, sizeof(cl_int), &resized_w);
    clSetKernelArg(kernel4, 3, sizeof(cl_int), &resized_h);
    clSetKernelArg(kernel4, 4, sizeof(cl_int), &neighbourhood_range);
    clSetKernelArg(kernel4, 5, sizeof(cl_int), &window);

    clEnqueueNDRangeKernel(queue,kernel4,2,NULL,global2, NULL,0, NULL,NULL);
    clFinish(queue);

    clEnqueueReadBuffer(queue,occlusionBufferL,CL_TRUE,0,resized_w*resized_h*sizeof(unsigned char),occlusionL,0,NULL,NULL);

    lodepng_encode_file("../Output_images/occlusion_filled.png", occlusionL, resized_w, resized_h, LCT_GREY, 8);
    printf("Occlusion filled output image saved.\n");


    gettimeofday(&t[4], NULL);
    gettimeofday(&t[5], NULL);

    // Profiling
    long seconds_1 = t[1].tv_sec - t[0].tv_sec;
    long useconds_1 = t[1].tv_usec - t[0].tv_usec;
    if(useconds_1 < 0){seconds_1--; useconds_1 += 1e6;}
    long milliseconds_1 = seconds_1 * 1000.0 + useconds_1 / 1000.0;

    long seconds_2 = t[2].tv_sec - t[1].tv_sec;
    long useconds_2 = t[2].tv_usec - t[1].tv_usec;
    if(useconds_2 < 0){seconds_2--; useconds_2 += 1e6;}
    long milliseconds_2 = seconds_2 * 1000.0 + useconds_2 / 1000.0;

    long seconds_3 = t[3].tv_sec - t[2].tv_sec;
    long useconds_3 = t[3].tv_usec - t[2].tv_usec;
    if(useconds_3 < 0){seconds_3--; useconds_3 += 1e6;}
    long milliseconds_3 = seconds_3 * 1000.0 + useconds_3 / 1000.0;

    long seconds_4 = t[4].tv_sec - t[3].tv_sec;
    long useconds_4 = t[4].tv_usec - t[3].tv_usec;
    if(useconds_4 < 0){seconds_4--; useconds_4 += 1e6;}
    long milliseconds_4 = seconds_4 * 1000.0 + useconds_4 / 1000.0;

    printf("Grayscale and resize (milliseconds): %ld\n", milliseconds_1);
    printf("Zncc (milliseconds): %ld\n", milliseconds_2);
    printf("Croschecking (milliseconds): %ld\n", milliseconds_3);
    printf("Occlusion filling (milliseconds): %ld\n", milliseconds_4);

    /* ---------- CLEANUP ---------- */
    clReleaseKernel(kernel1);
    clReleaseKernel(kernel2);
    clReleaseProgram(program1);
    clReleaseMemObject(leftBuffer);
    clReleaseMemObject(rightBuffer);
    clReleaseMemObject(tinyGrayBufferL);
    clReleaseMemObject(tinyGrayBufferR);
    clReleaseMemObject(disparityBufferL);
    clReleaseMemObject(disparityBufferR);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);

    free(inputImageLeft);
    free(inputImageRight);
    free(tinyGrayLeft);
    free(tinyGrayRight);
    free(disparityL);
    free(disparityR);
    free(crosscheckedL);
    free(occlusionL);

    return 0;
}
