#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <CL/cl.h>
#include "lodepng.h"
#include <sys/time.h>

// Input image dimensions and storage format.
#define WIDTH 2940
#define HEIGHT 2016
#define CHANNELS 4

// Stereo pipeline tuning parameters.
// ZNCC_MAX_DISPARITY_FULL_RES: largest disparity expected in the original full-resolution input.
// ZNCC_MAX_DISPARITY: disparity search range used on the downsampled images.
// ZNCC_WINDOW_RADIUS: half-width of the ZNCC support window on the downsampled images.
// CROSS_CHECK_THRESHOLD_PIXELS: largest allowed left/right disparity mismatch in disparity pixels.
// OCCLUSION_FILL_RANGE_FULL_RES: horizontal search distance used for filling invalid pixels at full resolution.
#define BLOCK_SIZE 8 // Workgroup size of the occlusion fill kernel.

bool useLocal = true; // Default to local memory kernel
#define ZNCC_MAX_DISPARITY_FULL_RES 256
#define ZNCC_MAX_DISPARITY (ZNCC_MAX_DISPARITY_FULL_RES / SCALE)
#define ZNCC_WINDOW_RADIUS 4
#define CROSS_CHECK_THRESHOLD_PIXELS 5
#define OCCLUSION_FILL_RANGE_FULL_RES 16

#define SCALE 4
#define RESIZED_WIDTH (WIDTH / SCALE)
#define RESIZED_HEIGHT (HEIGHT / SCALE)
// The matching pipeline runs on downsampled images, so the fill range is scaled to the resized resolution.
#define OCCLUSION_FILL_RANGE (OCCLUSION_FILL_RANGE_FULL_RES / SCALE)

typedef unsigned char BYTE;
struct timeval t[6];

long elapsedMs(const timeval& start, const timeval& end)
{
    long seconds = end.tv_sec - start.tv_sec;
    long useconds = end.tv_usec - start.tv_usec;
    if(useconds < 0){seconds--; useconds += 1000000;}
    return seconds * 1000 + useconds / 1000;
}

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

int main(int argc, char* argv[])
{
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--global") {
            useLocal = false;
        } else if (arg == "--local") {
            useLocal = true;
        } else {
            std::cerr << "Usage: " << argv[0] << " [--local|--global]" << std::endl;
            return -1;
        }
    }

    const char imgPathLeft[] = "../im0.png";
    const char imgPathRight[] = "../im1.png";
    const char znccOutputPathLeft[] = "../Output_images/depth_zncc_left.png";
    const char znccOutputPathRight[] = "../Output_images/depth_zncc_right.png";

    unsigned int w = WIDTH;
    unsigned int h = HEIGHT;

    unsigned char* inputImageLeft = (unsigned char*)malloc(w * h * 4);
    unsigned char* inputImageRight = (unsigned char*)malloc(w * h * 4);

    unsigned error;

    error = lodepng_decode_file(&inputImageLeft, &w, &h, imgPathLeft, LCT_RGBA, 8);
    if(error) { std::cout << "Error loading left image\n"; return -1; }

    error = lodepng_decode_file(&inputImageRight, &w, &h, imgPathRight, LCT_RGBA, 8);
    if(error) { std::cout << "Error loading right image\n"; return -1; }

    std::cout << "Images loaded: " << w << " x " << h << std::endl;

    /* ---------- OPENCL SETUP ---------- */
    cl_int err;
    cl_uint num_platforms;
    clGetPlatformIDs(0, nullptr, &num_platforms);
    std::vector<cl_platform_id> platforms(num_platforms);
    clGetPlatformIDs(num_platforms, platforms.data(), nullptr);

    cl_platform_id platform = nullptr;
    for(cl_uint i=0;i<num_platforms;i++)
    {
        char name[128];
        clGetPlatformInfo(platforms[i], CL_PLATFORM_NAME, sizeof(name), name, nullptr);
        std::cout<<"Found platform: "<<name<<std::endl;
        if(std::string(name).find("NVIDIA") != std::string::npos)
            platform = platforms[i];
    }
    if(platform == nullptr) { std::cout<<"No NVIDIA platform found\n"; return -1; }

    cl_device_id device;
    err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, nullptr);
    if(err != CL_SUCCESS){ std::cout<<"Failed to get GPU device\n"; return -1; }
    std::cout<<"GPU device acquired\n";

    cl_context context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &err);
    cl_command_queue queue = clCreateCommandQueueWithProperties(context, device, 0, &err);

    /* ---------- ORIGINAL BUFFERS ---------- */
    size_t imageSize = w*h*CHANNELS;
    cl_mem leftBuffer = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, imageSize, inputImageLeft, &err);
    cl_mem rightBuffer = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, imageSize, inputImageRight, &err);

    /* ---------- RESIZE BUFFERS ---------- */
    size_t resizedSize = RESIZED_WIDTH * RESIZED_HEIGHT * CHANNELS;

    cl_mem leftResizedBuffer = clCreateBuffer(context, CL_MEM_READ_WRITE, resizedSize, nullptr, &err);
    cl_mem rightResizedBuffer = clCreateBuffer(context, CL_MEM_READ_WRITE, resizedSize, nullptr, &err);

    /* ---------- RESIZE KERNEL ---------- */
    gettimeofday(&t[0], NULL);

    std::string resizeSrcStr = loadKernelSource("kernel_resize.cl");
    const char* resizeSrc = resizeSrcStr.c_str();
    size_t resizeSizeSrc = resizeSrcStr.length();

    cl_program resizeProgram = clCreateProgramWithSource(context, 1, &resizeSrc, &resizeSizeSrc, &err);
    err = clBuildProgram(resizeProgram, 1, &device, nullptr, nullptr, nullptr);

    cl_kernel resizeKernel = clCreateKernel(resizeProgram, "resize_downsample4", &err);

    int in_w = WIDTH;
    int in_h = HEIGHT;

    clSetKernelArg(resizeKernel, 2, sizeof(int), &in_w);
    clSetKernelArg(resizeKernel, 3, sizeof(int), &in_h);

    size_t globalResize[2] = {RESIZED_WIDTH, RESIZED_HEIGHT};

    // LEFT
    clSetKernelArg(resizeKernel, 0, sizeof(cl_mem), &leftBuffer);
    clSetKernelArg(resizeKernel, 1, sizeof(cl_mem), &leftResizedBuffer);
    clEnqueueNDRangeKernel(queue, resizeKernel, 2, nullptr, globalResize, nullptr, 0, nullptr, nullptr);

    // RIGHT
    clSetKernelArg(resizeKernel, 0, sizeof(cl_mem), &rightBuffer);
    clSetKernelArg(resizeKernel, 1, sizeof(cl_mem), &rightResizedBuffer);
    clEnqueueNDRangeKernel(queue, resizeKernel, 2, nullptr, globalResize, nullptr, 0, nullptr, nullptr);

    clFinish(queue);

    gettimeofday(&t[1], NULL);

    std::cout<<"Resized to "<<RESIZED_WIDTH<<" x "<<RESIZED_HEIGHT<<std::endl;

    /* ---------- USE RESIZED ---------- */
    unsigned int rw = RESIZED_WIDTH;
    unsigned int rh = RESIZED_HEIGHT;

    std::vector<unsigned char> disparityL(rw*rh);
    std::vector<unsigned char> disparityR(rw*rh);
    std::vector<unsigned char> crossChecked(rw*rh);
    std::vector<unsigned char> filledDisparity(rw*rh);

    cl_mem dispLBuffer = clCreateBuffer(context, CL_MEM_READ_WRITE, rw*rh, nullptr, &err);
    cl_mem dispRBuffer = clCreateBuffer(context, CL_MEM_READ_WRITE, rw*rh, nullptr, &err);
    cl_mem crossBuffer = clCreateBuffer(context, CL_MEM_READ_WRITE, rw*rh, nullptr, &err);
    cl_mem filledBuffer = clCreateBuffer(context, CL_MEM_READ_WRITE, rw*rh, nullptr, &err);

    size_t global[2] = {rw,rh};
    size_t znccLocal[2] = {16, 16};
    size_t znccGlobalLocal[2] = {
        ((rw + znccLocal[0] - 1) / znccLocal[0]) * znccLocal[0],
        ((rh + znccLocal[1] - 1) / znccLocal[1]) * znccLocal[1]
    };

    /* ---------- ZNCC ---------- */

    gettimeofday(&t[1], NULL);

    std::string kernelFile = useLocal ? "kernel_local.cl" : "kernel_global.cl";
    std::string kernelName = useLocal ? "zncc_local" : "zncc_global";

    std::string kernelSource1 = loadKernelSource(kernelFile.c_str());
    const char* src1 = kernelSource1.c_str();
    size_t src1size = kernelSource1.length();
    cl_program program1 = clCreateProgramWithSource(context, 1, &src1, &src1size, &err);
    std::string znccBuildOptions =
        "-D ZNCC_MAX_DISPARITY=" + std::to_string(ZNCC_MAX_DISPARITY) +
        " -D ZNCC_WINDOW_RADIUS=" + std::to_string(ZNCC_WINDOW_RADIUS);
    err = clBuildProgram(program1, 1, &device, znccBuildOptions.c_str(), nullptr, nullptr);

    cl_kernel kernel1 = clCreateKernel(program1, kernelName.c_str(), &err);

    clSetKernelArg(kernel1, 4, sizeof(int), &rw);
    clSetKernelArg(kernel1, 5, sizeof(int), &rh);

    int disp_sign = 1;
    clSetKernelArg(kernel1, 0, sizeof(cl_mem), &leftResizedBuffer);
    clSetKernelArg(kernel1, 1, sizeof(cl_mem), &rightResizedBuffer);
    clSetKernelArg(kernel1, 2, sizeof(cl_mem), &dispLBuffer);
    clSetKernelArg(kernel1, 3, sizeof(int), &disp_sign);

    err = clEnqueueNDRangeKernel(
        queue,
        kernel1,
        2,
        nullptr,
        useLocal ? znccGlobalLocal : global,
        useLocal ? znccLocal : nullptr,
        0,
        nullptr,
        nullptr
    );
    if (err != CL_SUCCESS) {
        std::cerr << "Failed to enqueue ZNCC left kernel: " << err << std::endl;
        return -1;
    }
    clFinish(queue);

    disp_sign = -1;
    clSetKernelArg(kernel1, 0, sizeof(cl_mem), &rightResizedBuffer);
    clSetKernelArg(kernel1, 1, sizeof(cl_mem), &leftResizedBuffer);
    clSetKernelArg(kernel1, 2, sizeof(cl_mem), &dispRBuffer);
    clSetKernelArg(kernel1, 3, sizeof(int), &disp_sign);

    err = clEnqueueNDRangeKernel(
        queue,
        kernel1,
        2,
        nullptr,
        useLocal ? znccGlobalLocal : global,
        useLocal ? znccLocal : nullptr,
        0,
        nullptr,
        nullptr
    );
    if (err != CL_SUCCESS) {
        std::cerr << "Failed to enqueue ZNCC right kernel: " << err << std::endl;
        return -1;
    }
    clFinish(queue);

    // Save the raw left/right ZNCC disparity maps before cross-checking and occlusion filling.
    clEnqueueReadBuffer(queue, dispLBuffer, CL_TRUE, 0, rw*rh, disparityL.data(), 0, nullptr, nullptr);
    clEnqueueReadBuffer(queue, dispRBuffer, CL_TRUE, 0, rw*rh, disparityR.data(), 0, nullptr, nullptr);
    lodepng_encode_file(znccOutputPathLeft, disparityL.data(), rw, rh, LCT_GREY, 8);
    lodepng_encode_file(znccOutputPathRight, disparityR.data(), rw, rh, LCT_GREY, 8);

    gettimeofday(&t[2], NULL);

    /* ---------- CROSS CHECK ---------- */

    gettimeofday(&t[2], NULL);

    std::string crossSrcStr = loadKernelSource("kernel_cross_check.cl");
    const char* crossSrc = crossSrcStr.c_str();
    size_t crossSize = crossSrcStr.length();
    cl_program crossProgram = clCreateProgramWithSource(context, 1, &crossSrc, &crossSize, &err);
    err = clBuildProgram(crossProgram, 1, &device, nullptr, nullptr, nullptr);

    cl_kernel crossKernel = clCreateKernel(crossProgram, "cross_check", &err);

    int crossCheckThresholdPixels = CROSS_CHECK_THRESHOLD_PIXELS;
    int maxDisparity = ZNCC_MAX_DISPARITY;

    clSetKernelArg(crossKernel, 0, sizeof(cl_mem), &dispLBuffer);
    clSetKernelArg(crossKernel, 1, sizeof(cl_mem), &dispRBuffer);
    clSetKernelArg(crossKernel, 2, sizeof(cl_mem), &crossBuffer);
    clSetKernelArg(crossKernel, 3, sizeof(int), &crossCheckThresholdPixels);
    clSetKernelArg(crossKernel, 4, sizeof(int), &maxDisparity);
    clSetKernelArg(crossKernel, 5, sizeof(int), &rw);
    clSetKernelArg(crossKernel, 6, sizeof(int), &rh);

    clEnqueueNDRangeKernel(queue, crossKernel, 2, nullptr, global, nullptr, 0, nullptr, nullptr);
    clFinish(queue);

    gettimeofday(&t[3], NULL);

    /* ---------- OCCLUSION FILL ---------- */

    gettimeofday(&t[3], NULL);

    std::string occKernelSrc = loadKernelSource("kernel_occlusion_fill.cl");
    const char* occSrc = occKernelSrc.c_str();
    size_t occSize = occKernelSrc.length();
    cl_program occProgram = clCreateProgramWithSource(context, 1, &occSrc, &occSize, &err);
    err = clBuildProgram(occProgram, 1, &device, nullptr, nullptr, nullptr);

    cl_kernel occKernel = clCreateKernel(occProgram, "occlusion_fill_opt", &err);

    int occlusionFillRange = OCCLUSION_FILL_RANGE;

    clSetKernelArg(occKernel, 0, sizeof(cl_mem), &crossBuffer);
    clSetKernelArg(occKernel, 1, sizeof(cl_mem), &filledBuffer);
    clSetKernelArg(occKernel, 2, sizeof(int), &rw);
    clSetKernelArg(occKernel, 3, sizeof(int), &rh);
    clSetKernelArg(occKernel, 4, sizeof(int), &occlusionFillRange);

    size_t local[2] = {BLOCK_SIZE, BLOCK_SIZE};
    size_t globalFill[2] = {
        ((rw+BLOCK_SIZE-1)/BLOCK_SIZE)*BLOCK_SIZE,
        ((rh+BLOCK_SIZE-1)/BLOCK_SIZE)*BLOCK_SIZE
    };

    clEnqueueNDRangeKernel(queue, occKernel, 2, nullptr, globalFill, local, 0, nullptr, nullptr);
    clFinish(queue);

    clEnqueueReadBuffer(queue, filledBuffer, CL_TRUE, 0, rw*rh, filledDisparity.data(), 0, nullptr, nullptr);

    gettimeofday(&t[4], NULL);

    /* ---------- SAVE ---------- */
    lodepng_encode_file("../Output_images/depth_filled.png", filledDisparity.data(), rw, rh, LCT_GREY, 8);

    long ms1 = elapsedMs(t[0], t[1]);
    long ms2 = elapsedMs(t[1], t[2]);
    long ms3 = elapsedMs(t[2], t[3]);
    long ms4 = elapsedMs(t[3], t[4]);

    printf("Resize + grayscale: %ld ms\n", ms1);
    printf("ZNCC: %ld ms\n", ms2);
    printf("Cross-check: %ld ms\n", ms3);
    printf("Occlusion fill: %ld ms\n", ms4);
    
    std::cout<<"DONE\n";

    return 0;
}
