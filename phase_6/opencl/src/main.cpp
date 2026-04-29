
#ifdef __linux__ 
    #include <limits.h>
    #include <unistd.h>
    #include <filesystem>
    #include <iostream>
    #include <fstream>
    #include <vector>
    #include <CL/cl.h>
    #include "lodepng.h"
    #include <sys/time.h>
    //linux code goes here
#elif _WIN32
    #include <filesystem>
    #include <iostream>
    #include <fstream>
    #include <vector>
    #include <CL/cl.h>
    #include "lodepng.h"
    #include "sys/time.h"
    // windows code goes here
#endif

#define WIDTH 2940
#define HEIGHT 2016
#define WINDOW 13
#define CHANNELS 4
#define MAX_DISPARITY 64
#define RESIZE_FACTOR 4
#define BLOCK_SIZE 16
#define BLOCK_WIDTH 32
#define BLOCK_HEIGHT 8

typedef unsigned char BYTE;

struct timeval t[6];
struct timeval pixeltime[2];

std::filesystem::path getExecutableFolderPath() {
#if defined(_WIN32)
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::string spath = std::string(path);
    int pos = spath.find_last_of("\\/");
    
    std::filesystem::path execPath = spath.substr(0, pos + 1);

    return execPath;

#elif defined(__linux__)
    char result[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    if (count != -1) {
        result[count] = '\0';  // null-terminate
    } else {
        perror("readlink");
    }

    std::string spath = std::string(result);
    int pos = spath.find_last_of("\\/");
    std::filesystem::path execPath = spath.substr(0, pos + 1);

    return execPath ;

#else
    return {};
#endif
    }

std::string loadKernelSource(std::string &src_path)
{
    std::ifstream file(src_path);
    if(!file.is_open())
    {
        std::cerr << "Failed to open kernel file: " << src_path << std::endl;
        exit(1);
    }
    return std::string(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    );
}
void inline print_help()
    {
    printf("./opencl [-il] [Left input image path] [-ir] [Right input image path] [-k] [kernel code source file path] [-o] [Output images path] [-w] [kernel window size] [-d] [maximum disparity] [-t] [cross checking threshold] [-nr] [occlusion filling neighbourhood range]\n");
    }

void inline arg_error(char* endptr, char* argv_a)
    {
    if (endptr == argv_a || *endptr != '\0' || errno == ERANGE)
        {
        printf("Please provide a valid integer as a parameter for %s.\n\n", argv_a);
        print_help();
        exit(1);
        }
    }

int main(int argc, char **argv)
{



    std::string executableFolderPath {getExecutableFolderPath().string()};

    char imgPathLeft[256]{};
    char imgPathRight[256]{};
    char outputImagesPath[256]{};
    char kernelPath[256]{};

    std::string imgPathLeftStr = executableFolderPath + "../img/im0.png";
    std::string imgPathRightStr = executableFolderPath + "../img/im1.png";
    std::string outputPathStr = executableFolderPath + "../Output_images/";
    std::string kernelPathStr = executableFolderPath + "../src/kernels.cl";


    strncpy(imgPathLeft, imgPathLeftStr.c_str(), imgPathLeftStr.length());
    strncpy(imgPathRight, imgPathRightStr.c_str(), imgPathRightStr.length());
    strncpy(outputImagesPath, outputPathStr.c_str(), outputPathStr.length());
    strncpy(kernelPath, kernelPathStr.c_str(), kernelPathStr.length());


    unsigned int w = WIDTH;
    unsigned int h = HEIGHT;
    unsigned int d = MAX_DISPARITY;
    int threshold = 10;
    int neighbourhood_range = 50;

    int window = WINDOW;
    unsigned int resize_factor = RESIZE_FACTOR;
    unsigned int resized_w = w / resize_factor;
    unsigned int resized_h = h / resize_factor;
    unsigned int resized_w_padded = w / resize_factor + 2;
    unsigned int resized_h_padded = h / resize_factor + 2;

    char* endptr;

    if (argc > 1)
        {
        for (int a = 1; a < argc; a++)
            {
            if (strcmp(argv[a], "-w") == 0)
                {
                window = strtol(argv[a + 1], &endptr, 10);
                arg_error(endptr, argv[a]);
                a++;
                }
            else if (strcmp(argv[a], "-d") == 0)
                {
                d = strtol(argv[a + 1], &endptr, 10);
                arg_error(endptr, argv[a]);
                a++;
                }
            else if (strcmp(argv[a], "-t") == 0)
                {
                threshold = strtol(argv[a + 1], &endptr, 10);
                arg_error(endptr, argv[a]);
                a++;
                }
            else if (strcmp(argv[a], "-nr") == 0)
                {
                neighbourhood_range = strtol(argv[a + 1], &endptr, 10);
                arg_error(endptr, argv[a]);
                a++;
                }
            else if (strcmp(argv[a], "-il") == 0)
                {
                memset(imgPathLeft, 0, 256);
                snprintf(imgPathLeft, sizeof(imgPathLeft), "%s", argv[a + 1]);
                //strncpy(imgPathLeft, argv[a + 1], sizeof(imgPathLeft) - 1);
                a++;
                }
            else if (strcmp(argv[a], "-ir") == 0)
                {
                memset(imgPathRight, 0, 256);
                snprintf(imgPathRight, sizeof(imgPathRight), "%s", argv[a + 1]);
                //strncpy(imgPathRight, argv[a + 1], sizeof(imgPathRight) - 1);
                a++;
                }
            else if (strcmp(argv[a], "-k") == 0)
                {
                memset(kernelPath, 0, 256);
                snprintf(kernelPath, sizeof(kernelPath), "%s", argv[a + 1]);
                //strncpy(kernelPath, argv[a + 1], sizeof(kernelPath) - 1);
                a++;
                }
            else if (strcmp(argv[a], "-o") == 0)
                {
                memset(outputImagesPath, 0, 256);
                snprintf(outputImagesPath, sizeof(outputImagesPath), "%s", argv[a + 1]);
                //strncpy(outputImagesPath, argv[a + 1], sizeof(outputImagesPath) - 1);
                a++;
                }
            else if (strcmp(argv[a], "-h") == 0)
                {
                print_help();
                }
            }
        }

    kernelPathStr = std::string(kernelPath);
    //unsigned char* inputImageLeft = NULL;
    //unsigned char* inputImageRight = NULL;

    unsigned char* inputImageLeft = (unsigned char*)malloc(w * h * 4 * sizeof(unsigned char));
    unsigned char* inputImageRight = (unsigned char*)malloc(w * h * 4 * sizeof(unsigned char));
    unsigned char* tinyGrayLeft = (unsigned char *)malloc(w/resize_factor * h/resize_factor * sizeof(unsigned char));
    unsigned char* tinyGrayRight = (unsigned char *)malloc(w/resize_factor * h/resize_factor * sizeof(unsigned char));

    // Add 1 pixel padding to resized images. Needed for integral image processing.
    unsigned char* tinyGrayLeftPadded = (unsigned char*)calloc((w / resize_factor + 2) * (h / resize_factor + 2), sizeof(BYTE));
    unsigned char* tinyGrayRightPadded = (unsigned char*)calloc((w / resize_factor + 2) * (h / resize_factor + 2), sizeof(BYTE));

    unsigned char* disparityL = (unsigned char *)calloc((w / resize_factor + 2) * (h / resize_factor + 2), sizeof(BYTE));
    unsigned char* disparityR = (unsigned char *)calloc((w / resize_factor + 2) * (h / resize_factor + 2), sizeof(BYTE));
    unsigned char *crosscheckedL = (unsigned char *)calloc((w/resize_factor + 2) * (h/resize_factor + 2), sizeof(BYTE));
    unsigned char *occlusionL = (unsigned char *)calloc((w/resize_factor + 2) * (h/resize_factor + 2), sizeof(BYTE));

    // Integral image tables with one pixel padding
    uint32_t* inputIntegralL = (uint32_t*)calloc((w / resize_factor + 2) * (h / resize_factor + 2), sizeof(uint32_t));
    uint32_t* inputIntegralR = (uint32_t*)calloc((w / resize_factor + 2) * (h / resize_factor + 2), sizeof(uint32_t));
    uint64_t* inputIntegralSquaredL = (uint64_t*)calloc((w / resize_factor + 2) * (h / resize_factor + 2), sizeof(uint64_t));
    uint64_t* inputIntegralSquaredR = (uint64_t*)calloc((w / resize_factor + 2) * (h / resize_factor + 2), sizeof(uint64_t));

    uint32_t* inputIntegralTempL = (uint32_t*)calloc((w / resize_factor + 2) * (h / resize_factor + 2), sizeof(uint32_t));
    uint32_t* inputIntegralTempR = (uint32_t*)calloc((w / resize_factor + 2) * (h / resize_factor + 2), sizeof(uint32_t));
    uint64_t* inputIntegralSquaredTempL = (uint64_t*)calloc((w / resize_factor + 2) * (h / resize_factor + 2), sizeof(uint64_t));
    uint64_t* inputIntegralSquaredTempR = (uint64_t*)calloc((w / resize_factor + 2) * (h / resize_factor + 2), sizeof(uint64_t));

    float* meanTableL = (float*)calloc((w / resize_factor + 2) * (h / resize_factor + 2), sizeof(float));
    float* meanTableR = (float*)calloc((w / resize_factor + 2) * (h / resize_factor + 2), sizeof(float));
    float* varTableL = (float*)calloc((w / resize_factor + 2) * (h / resize_factor + 2), sizeof(float));
    float* varTableR = (float*)calloc((w / resize_factor + 2) * (h / resize_factor + 2), sizeof(float));


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
        else if(std::string(name).find("AMD")!=std::string::npos)
            platform = platforms[i];
    }
    if(platform==NULL) { std::cout<<"No NVIDIA or AMD platform found\n"; return -1; }

    cl_device_id device;
    err = clGetDeviceIDs(platform,CL_DEVICE_TYPE_GPU,1,&device,NULL);
    if(err!=CL_SUCCESS){ std::cout<<"Failed to get GPU device\n"; return -1; }
    std::cout<<"GPU device acquired\n";

    char device_name[128];
    clGetDeviceInfo(device,CL_DEVICE_NAME,sizeof(device_name),device_name,NULL);
    std::cout<<"Device: "<<device_name<<std::endl;

    cl_context context = clCreateContext(NULL,1,&device,NULL,NULL,&err);
    cl_command_queue queue = clCreateCommandQueueWithProperties(context,device,0,&err);

    /* ---------- BUFFERS ---------- */

    size_t imageSize = w*h*CHANNELS * sizeof(unsigned char);
    size_t tinyGrayImageSize = w/4 * h/4 * sizeof(unsigned char);
    size_t tinyGrayPaddedImageSize = (w/resize_factor + 2) * (h/resize_factor + 2) * sizeof(unsigned char);
    size_t tinyGrayPaddedImageSize32 = (w/resize_factor + 2) * (h/resize_factor + 2) * sizeof(unsigned int);
    size_t tinyGrayPaddedImageSize64 = (w/resize_factor + 2) * (h/resize_factor + 2) * sizeof(unsigned long);

    cl_mem leftBuffer = clCreateBuffer(context,CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR,imageSize,inputImageLeft,&err);
    cl_mem rightBuffer = clCreateBuffer(context,CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR,imageSize,inputImageRight,&err);
    cl_mem tinyGrayBufferL = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, tinyGrayImageSize, tinyGrayLeft, &err);
    cl_mem tinyGrayBufferR = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, tinyGrayImageSize, tinyGrayRight, &err);
    cl_mem tinyGrayPaddedBufferL = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, tinyGrayPaddedImageSize, tinyGrayLeftPadded , &err);
    cl_mem tinyGrayPaddedBufferR = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, tinyGrayPaddedImageSize, tinyGrayRightPadded , &err);

    cl_mem disparityBufferL = clCreateBuffer(context,CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,tinyGrayPaddedImageSize,disparityL,&err);
    cl_mem disparityBufferR = clCreateBuffer(context,CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,tinyGrayPaddedImageSize,disparityR,&err);
    cl_mem crosscheckedBufL = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, tinyGrayPaddedImageSize, crosscheckedL, &err); 
    cl_mem occlusionBufferL = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, tinyGrayPaddedImageSize, occlusionL, &err); 

    cl_mem inputIntegralBufferL = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, tinyGrayPaddedImageSize32, inputIntegralL, &err);
    cl_mem inputIntegralBufferR = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, tinyGrayPaddedImageSize32, inputIntegralR, &err);
    cl_mem inputIntegralSquaredBufferL = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, tinyGrayPaddedImageSize64, inputIntegralSquaredL, &err);
    cl_mem inputIntegralSquaredBufferR = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, tinyGrayPaddedImageSize64, inputIntegralSquaredR, &err);
    cl_mem inputIntegralTempBufferL = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, tinyGrayPaddedImageSize32, inputIntegralTempL, &err);
    cl_mem inputIntegralTempBufferR = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, tinyGrayPaddedImageSize32, inputIntegralTempR, &err);
    cl_mem inputIntegralSquaredTempBufferL = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, tinyGrayPaddedImageSize64, inputIntegralSquaredTempL, &err);
    cl_mem inputIntegralSquaredTempBufferR = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, tinyGrayPaddedImageSize64, inputIntegralSquaredTempR, &err);

    cl_mem meanTableBufferL = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, tinyGrayPaddedImageSize32, meanTableL, &err);
    cl_mem meanTableBufferR = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, tinyGrayPaddedImageSize32, meanTableR, &err);
    cl_mem varTableBufferL = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, tinyGrayPaddedImageSize32, varTableL, &err);
    cl_mem varTableBufferR = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, tinyGrayPaddedImageSize32, varTableR, &err);

    // Load kernel functions
    std::string kernelSource1 = loadKernelSource(kernelPathStr);
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
    size_t global1[2] = {size_t(resized_w), size_t(resized_h)};

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


    std::string tinyGrayLeftPath = std::string(outputImagesPath) + "/tinyGrayLeft.png";
    std::string tinyGrayRightPath = std::string(outputImagesPath) + "/tinyGrayRight.png";

    lodepng_encode_file(tinyGrayLeftPath.c_str(), tinyGrayLeft, w / resize_factor, h / resize_factor, LCT_GREY, 8);
    lodepng_encode_file(tinyGrayRightPath.c_str(), tinyGrayRight, w/resize_factor, h/resize_factor, LCT_GREY, 8); 

    cl_kernel kernel5 = clCreateKernel(program1,"pad_image_1px",&err);
    clSetKernelArg(kernel5,0,sizeof(cl_mem),&tinyGrayBufferL);
    clSetKernelArg(kernel5,1,sizeof(cl_mem),&tinyGrayBufferR);
    clSetKernelArg(kernel5,2,sizeof(cl_uint),&resized_w);
    clSetKernelArg(kernel5,3,sizeof(cl_uint),&resized_h);
    clSetKernelArg(kernel5,4,sizeof(cl_mem),&tinyGrayPaddedBufferL);
    clSetKernelArg(kernel5,5,sizeof(cl_mem),&tinyGrayPaddedBufferR);

    clEnqueueNDRangeKernel(queue,kernel5,2,NULL,global1,NULL,0,NULL,NULL);
    clFinish(queue);

    clEnqueueReadBuffer(queue,tinyGrayPaddedBufferL,CL_TRUE,0,(w/resize_factor + 2) * (h/resize_factor + 2) * sizeof(unsigned char),tinyGrayLeftPadded,0,NULL,NULL);
    clEnqueueReadBuffer(queue,tinyGrayPaddedBufferR,CL_TRUE,0,(w/resize_factor + 2) * (h/resize_factor + 2) * sizeof(unsigned char),tinyGrayRightPadded,0,NULL,NULL);


    std::string tinyGrayLeftPaddedPath = std::string(outputImagesPath) + "/tinyGrayLeftPadded.png";
    std::string tinyGrayRightPaddedPath = std::string(outputImagesPath) + "/tinyGrayRightPadded.png";

    lodepng_encode_file(tinyGrayLeftPaddedPath.c_str(), tinyGrayLeftPadded, w / resize_factor + 2, h / resize_factor + 2, LCT_GREY, 8);
    lodepng_encode_file(tinyGrayRightPaddedPath.c_str(), tinyGrayRightPadded, w/resize_factor + 2, h/resize_factor + 2, LCT_GREY, 8); 

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
   
    /* ---------- KERNEL 6-8 INTEGRAL IMAGE TABLES and MEAN VAR TABLES ---------- */
    size_t global_integral_row[1] = {size_t(resized_h_padded)};

    cl_kernel kernel6 = clCreateKernel(program1, "populate_integral_tables_row", &err);
    clSetKernelArg(kernel6, 0, sizeof(cl_mem), &tinyGrayPaddedBufferL);
    clSetKernelArg(kernel6, 1, sizeof(cl_mem), &tinyGrayPaddedBufferR);
    clSetKernelArg(kernel6, 2, sizeof(cl_mem), &inputIntegralTempBufferL);
    clSetKernelArg(kernel6, 3, sizeof(cl_mem), &inputIntegralTempBufferR);
    clSetKernelArg(kernel6, 4, sizeof(cl_mem), &inputIntegralSquaredTempBufferL);
    clSetKernelArg(kernel6, 5, sizeof(cl_mem), &inputIntegralSquaredTempBufferR);
    clSetKernelArg(kernel6, 6,sizeof(cl_int),&resized_w_padded);
    clSetKernelArg(kernel6, 7,sizeof(cl_int),&resized_h_padded);

    clEnqueueNDRangeKernel(queue, kernel6, 1, NULL, global_integral_row, NULL, 0, NULL, NULL);
    clFinish(queue);

    size_t global_integral_col[1] = {size_t(resized_w_padded)};
    cl_kernel kernel7 = clCreateKernel(program1, "populate_integral_tables_col", &err);
    clSetKernelArg(kernel7, 0, sizeof(cl_mem), &inputIntegralTempBufferL);
    clSetKernelArg(kernel7, 1, sizeof(cl_mem), &inputIntegralTempBufferR);
    clSetKernelArg(kernel7, 2, sizeof(cl_mem), &inputIntegralSquaredTempBufferL);
    clSetKernelArg(kernel7, 3, sizeof(cl_mem), &inputIntegralSquaredTempBufferR);
    clSetKernelArg(kernel7, 4, sizeof(cl_mem), &inputIntegralBufferL);
    clSetKernelArg(kernel7, 5, sizeof(cl_mem), &inputIntegralBufferR);
    clSetKernelArg(kernel7, 6, sizeof(cl_mem), &inputIntegralSquaredBufferL);
    clSetKernelArg(kernel7, 7, sizeof(cl_mem), &inputIntegralSquaredBufferR);
    clSetKernelArg(kernel7, 8,sizeof(cl_int),&resized_w_padded);
    clSetKernelArg(kernel7, 9,sizeof(cl_int),&resized_h_padded);

    clEnqueueNDRangeKernel(queue, kernel7, 1, NULL, global_integral_col, NULL, 0, NULL, NULL);
    clFinish(queue);

    //size_t global2[3] = {size_t(resized_w),size_t(resized_h),size_t(d)};
    size_t global2[2] = {size_t(resized_w_padded),size_t(resized_h_padded)};
    //size_t global2[2] = {((resized_w + 15) / 16) * 16,((resized_h + 15) / 16) * 16};
    //size_t local2[2] = {16, 16};
    
    cl_kernel kernel8 = clCreateKernel(program1, "populate_mean_var_tables", &err);
    clSetKernelArg(kernel8, 0, sizeof(cl_mem), &inputIntegralBufferL);
    clSetKernelArg(kernel8, 1, sizeof(cl_mem), &inputIntegralBufferR);
    clSetKernelArg(kernel8, 2, sizeof(cl_mem), &inputIntegralSquaredBufferL);
    clSetKernelArg(kernel8, 3, sizeof(cl_mem), &inputIntegralSquaredBufferR);
    clSetKernelArg(kernel8, 4, sizeof(cl_mem), &meanTableBufferL);
    clSetKernelArg(kernel8, 5, sizeof(cl_mem), &meanTableBufferR);
    clSetKernelArg(kernel8, 6, sizeof(cl_mem), &varTableBufferL);
    clSetKernelArg(kernel8, 7, sizeof(cl_mem), &varTableBufferR);
    clSetKernelArg(kernel8, 8, sizeof(cl_int),&resized_w_padded);
    clSetKernelArg(kernel8, 9, sizeof(cl_int),&resized_h_padded);
    clSetKernelArg(kernel8, 10, sizeof(cl_int),&window); 

    clEnqueueNDRangeKernel(queue,kernel8,2,NULL,global2, NULL,0,NULL,NULL);
    clFinish(queue);

    // debugging
    //clEnqueueReadBuffer(queue, inputIntegralBufferL, CL_TRUE, 0, resized_w_padded * resized_h_padded * sizeof(unsigned int), inputIntegralL, 0, NULL, NULL);
    //clEnqueueReadBuffer(queue, inputIntegralSquaredBufferL, CL_TRUE, 0, resized_w_padded * resized_h_padded * sizeof(unsigned long), inputIntegralSquaredL, 0, NULL, NULL);
    //clEnqueueReadBuffer(queue, meanTableBufferL, CL_TRUE, 0, resized_w_padded * resized_h_padded * sizeof(float), meanTableL, 0, NULL, NULL);
    //clEnqueueReadBuffer(queue, varTableBufferL, CL_TRUE, 0, resized_w_padded * resized_h_padded * sizeof(float), varTableL, 0, NULL, NULL);

    /* ---------- KERNEL 1 FAST ---------- */
    int disp_sign = 1;

    //std::string kernelSource1 = loadKernelSource("kernel_fast.cl");

    clEnqueueWriteBuffer(queue, debugIndex, CL_TRUE, 0, sizeof(int), &zero, 0, NULL, NULL);


    size_t global3[2] = {((resized_w_padded + BLOCK_WIDTH - 1) / BLOCK_WIDTH) * BLOCK_WIDTH,((resized_h_padded + BLOCK_HEIGHT -1) / BLOCK_HEIGHT) * BLOCK_HEIGHT};
    size_t local3[2] = {BLOCK_WIDTH, BLOCK_HEIGHT};

    int r = window / 2;
    int tileL_h = BLOCK_HEIGHT + 2 * r;
    int tileL_w = BLOCK_WIDTH + 2 * r;
    int tileR_h = BLOCK_HEIGHT + 2 * r;
    int tileR_w = BLOCK_WIDTH + MAX_DISPARITY + 2 * r;

    size_t localMemSizeL = tileL_w * tileL_h * sizeof(unsigned char);
    size_t localMemSizeR = tileR_w * tileR_h * sizeof(unsigned char);


    cl_kernel kernel2 = clCreateKernel(program1,"zncc_fast_integral",&err);
    clSetKernelArg(kernel2,0,sizeof(cl_mem),&tinyGrayPaddedBufferL);
    clSetKernelArg(kernel2,1,sizeof(cl_mem),&tinyGrayPaddedBufferR);
    clSetKernelArg(kernel2,2, sizeof(cl_mem), &meanTableBufferL);
    clSetKernelArg(kernel2,3, sizeof(cl_mem), &meanTableBufferR);
    clSetKernelArg(kernel2,4, sizeof(cl_mem), &varTableBufferL);
    clSetKernelArg(kernel2,5, sizeof(cl_mem), &varTableBufferR);
    clSetKernelArg(kernel2,6,sizeof(cl_mem),&disparityBufferL);
    clSetKernelArg(kernel2,7,sizeof(cl_int),&disp_sign);
    clSetKernelArg(kernel2,8,sizeof(cl_int),&resized_w_padded);
    clSetKernelArg(kernel2,9,sizeof(cl_int),&resized_h_padded);
    clSetKernelArg(kernel2,10, sizeof(cl_int),&window); 
    clSetKernelArg(kernel2,11, localMemSizeL, NULL);
    clSetKernelArg(kernel2,12, localMemSizeR, NULL);


    clEnqueueNDRangeKernel(queue,kernel2,2,NULL,global3, local3,0,NULL,NULL);
    clFinish(queue);

    clEnqueueReadBuffer(queue,disparityBufferL,CL_TRUE,0,resized_w_padded
            *resized_h_padded*sizeof(unsigned char),disparityL,0,NULL,NULL);

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

    lodepng_encode_file((std::string(outputImagesPath) + std::string("/depth1.png")).c_str(), disparityL, resized_w_padded, resized_h_padded, LCT_GREY, 8);
    std::cout<<"Depth map 1 saved\n";

    disp_sign = -1;

    kernel2 = clCreateKernel(program1,"zncc_fast_integral",&err);
    clSetKernelArg(kernel2,0,sizeof(cl_mem),&tinyGrayPaddedBufferR);
    clSetKernelArg(kernel2,1,sizeof(cl_mem),&tinyGrayPaddedBufferL);
    clSetKernelArg(kernel2,2, sizeof(cl_mem), &meanTableBufferR);
    clSetKernelArg(kernel2,3, sizeof(cl_mem), &meanTableBufferL);
    clSetKernelArg(kernel2,4, sizeof(cl_mem), &varTableBufferR);
    clSetKernelArg(kernel2,5, sizeof(cl_mem), &varTableBufferL);
    clSetKernelArg(kernel2,6,sizeof(cl_mem),&disparityBufferR);
    clSetKernelArg(kernel2,7,sizeof(cl_int),&disp_sign);
    clSetKernelArg(kernel2,8,sizeof(cl_int),&resized_w_padded);
    clSetKernelArg(kernel2,9,sizeof(cl_int),&resized_h_padded);
    clSetKernelArg(kernel2,10, sizeof(cl_int),&window); 
    clSetKernelArg(kernel2,11, localMemSizeL, NULL);
    clSetKernelArg(kernel2,12, localMemSizeR, NULL);

    clEnqueueNDRangeKernel(queue,kernel2,2,NULL,global3, local3,0, NULL,NULL);
    clFinish(queue);

    clEnqueueReadBuffer(queue,disparityBufferR,CL_TRUE,0,resized_w_padded*resized_h_padded*sizeof(unsigned char),disparityR,0,NULL,NULL);
    

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
    lodepng_encode_file((std::string(outputImagesPath) + std::string("/depth2.png")).c_str(), disparityR, resized_w_padded, resized_h_padded, LCT_GREY, 8);
    
    std::cout<<"Depth map 2 saved\n";
    gettimeofday(&t[2], NULL);

    
    clEnqueueWriteBuffer(queue, debugIndex, CL_TRUE, 0, sizeof(int), &zero, 0, NULL, NULL);


    cl_kernel kernel3 = clCreateKernel(program1, "cross_checking", &err);
    clSetKernelArg(kernel3, 0, sizeof(cl_mem), &disparityBufferL);
    clSetKernelArg(kernel3, 1, sizeof(cl_mem), &disparityBufferR);
    clSetKernelArg(kernel3, 2, sizeof(cl_mem), &crosscheckedBufL);
    clSetKernelArg(kernel3, 3, sizeof(cl_int), &resized_w_padded);
    clSetKernelArg(kernel3, 4, sizeof(cl_int), &resized_h_padded);
    clSetKernelArg(kernel3, 5, sizeof(cl_int), &threshold);
    clSetKernelArg(kernel3, 6, sizeof(cl_int), &window);
    clSetKernelArg(kernel3,7,sizeof(cl_mem),&debugBuf);
    clSetKernelArg(kernel3,8,sizeof(cl_mem),&debugIndex);

    clEnqueueNDRangeKernel(queue,kernel3,2,NULL,global2, NULL,0, NULL,NULL);
    clFinish(queue);

    clEnqueueReadBuffer(queue,crosscheckedBufL,CL_TRUE,0,resized_w_padded*resized_h_padded*sizeof(unsigned char),crosscheckedL,0,NULL,NULL);
    clEnqueueReadBuffer(queue, debugBuf, CL_TRUE, 0, 8192, buffer, 0, NULL, NULL);
    printf("%s\n", buffer);


    lodepng_encode_file((std::string(outputImagesPath) + std::string("/cross_checked.png")).c_str(), crosscheckedL, resized_w_padded, resized_h_padded, LCT_GREY, 8);
    printf("Cross checked output image saved.\n");


    gettimeofday(&t[3], NULL);


    cl_kernel kernel4 = clCreateKernel(program1, "occlusion_filling", &err);
    clSetKernelArg(kernel4, 0, sizeof(cl_mem), &occlusionBufferL);
    clSetKernelArg(kernel4, 1, sizeof(cl_mem), &crosscheckedBufL);
    clSetKernelArg(kernel4, 2, sizeof(cl_int), &resized_w_padded);
    clSetKernelArg(kernel4, 3, sizeof(cl_int), &resized_h_padded);
    clSetKernelArg(kernel4, 4, sizeof(cl_int), &neighbourhood_range);
    clSetKernelArg(kernel4, 5, sizeof(cl_int), &window);

    clEnqueueNDRangeKernel(queue,kernel4,2,NULL,global2, NULL,0, NULL,NULL);
    clFinish(queue);

    clEnqueueReadBuffer(queue,occlusionBufferL,CL_TRUE,0,resized_w_padded*resized_h_padded*sizeof(unsigned char),occlusionL,0,NULL,NULL);

    lodepng_encode_file((std::string(outputImagesPath) + std::string("/occlusion_filled.png")).c_str(), occlusionL, resized_w_padded, resized_h_padded, LCT_GREY, 8);
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
    clReleaseKernel(kernel3);
    clReleaseKernel(kernel4);
    clReleaseKernel(kernel5);
    clReleaseKernel(kernel6);
    clReleaseKernel(kernel7);
    clReleaseKernel(kernel8);

    clReleaseProgram(program1);

    clReleaseMemObject(leftBuffer);
    clReleaseMemObject(rightBuffer);
    clReleaseMemObject(tinyGrayBufferL);
    clReleaseMemObject(tinyGrayBufferR);
    clReleaseMemObject(tinyGrayPaddedBufferL);
    clReleaseMemObject(tinyGrayPaddedBufferR);
    clReleaseMemObject(inputIntegralTempBufferL);
    clReleaseMemObject(inputIntegralTempBufferR);
    clReleaseMemObject(inputIntegralSquaredTempBufferL);
    clReleaseMemObject(inputIntegralSquaredTempBufferR);
    clReleaseMemObject(inputIntegralBufferL);
    clReleaseMemObject(inputIntegralBufferR);
    clReleaseMemObject(inputIntegralSquaredBufferL);
    clReleaseMemObject(inputIntegralSquaredBufferR);

    clReleaseMemObject(meanTableBufferL);
    clReleaseMemObject(varTableBufferL);
    clReleaseMemObject(meanTableBufferR);
    clReleaseMemObject(varTableBufferR);

    clReleaseMemObject(disparityBufferL);
    clReleaseMemObject(disparityBufferR);
    clReleaseMemObject(crosscheckedBufL);
    clReleaseMemObject(occlusionBufferL);

    clReleaseCommandQueue(queue);
    clReleaseContext(context);

    free(inputImageLeft);
    free(inputImageRight);
    free(tinyGrayLeft);
    free(tinyGrayRight);
    free(tinyGrayLeftPadded);
    free(tinyGrayRightPadded);
    free(inputIntegralL);
    free(inputIntegralR);
    free(inputIntegralSquaredL);
    free(inputIntegralSquaredR);
    free(inputIntegralTempL);
    free(inputIntegralTempR);
    free(inputIntegralSquaredTempL);
    free(inputIntegralSquaredTempR);

    free(meanTableL);
    free(meanTableR);
    free(varTableL);
    free(varTableR);

    free(disparityL);
    free(disparityR);
    free(crosscheckedL);
    free(occlusionL);

    return 0;
}
