#include <CL/cl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define CHECK_ERROR(err, msg) if(err != CL_SUCCESS){ printf("%s (%d)\n", msg, err); exit(1); }

double current_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec*1000.0 + ts.tv_nsec/1e6;
}

// OpenCL kernel: matrix multiplication
const char* kernelSource = 
"__kernel void mat_mul(__global float* A, __global float* B, __global float* C, int N){\n"
"   int row = get_global_id(0);\n"
"   int col = get_global_id(1);\n"
"   float sum = 0.0f;\n"
"   for(int k=0;k<N;k++) sum += A[row*N + k] * B[k*N + col];\n"
"   C[row*N + col] = sum;\n"
"}\n";

int main() {
    cl_int err;

    // 1️⃣ Platform
    cl_platform_id platform;
    err = clGetPlatformIDs(1, &platform, NULL);
    CHECK_ERROR(err, "Failed to get platform");

    // 2️⃣ Device (CPU)
    cl_device_id device;
    err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_CPU, 1, &device, NULL);
    CHECK_ERROR(err, "Failed to get device");

    char deviceName[128];
    clGetDeviceInfo(device, deviceName, sizeof(deviceName), deviceName, NULL);
    printf("Using device: %s\n", deviceName);
    printf("OpenCL initialized OK\n");

    // 3️⃣ Context & Queue
    cl_context context = clCreateContext(NULL, 1, &device, NULL, NULL, &err);
    CHECK_ERROR(err, "Failed to create context");
    cl_command_queue queue = clCreateCommandQueue(context, device, CL_QUEUE_PROFILING_ENABLE, &err);
    CHECK_ERROR(err, "Failed to create queue");

    // 4️⃣ Matriisit (hardcoded)
    const int N = 4;
    float A[16] = {1,2,3,4, 5,6,7,8, 9,10,11,12, 13,14,15,16};
    float B[16] = {16,15,14,13, 12,11,10,9, 8,7,6,5, 4,3,2,1};
    float C[16];

    // 5️⃣ Buffers
    cl_mem bufA = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(float)*N*N, A, &err);
    cl_mem bufB = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(float)*N*N, B, &err);
    cl_mem bufC = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(float)*N*N, NULL, &err);

    // 6️⃣ Program & Kernel
    cl_program program = clCreateProgramWithSource(context,1,&kernelSource,NULL,&err);
    CHECK_ERROR(err,"Failed to create program");
    err = clBuildProgram(program,1,&device,NULL,NULL,NULL);
    if(err != CL_SUCCESS){
        size_t log_size;
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG,0,NULL,&log_size);
        char* log = (char*)malloc(log_size);
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, log_size, log,NULL);
        printf("Build log:\n%s\n", log);
        free(log);
        exit(1);
    }
    cl_kernel kernel = clCreateKernel(program,"mat_mul",&err);
    CHECK_ERROR(err,"Failed to create kernel");

    clSetKernelArg(kernel,0,sizeof(cl_mem),&bufA);
    clSetKernelArg(kernel,1,sizeof(cl_mem),&bufB);
    clSetKernelArg(kernel,2,sizeof(cl_mem),&bufC);
    clSetKernelArg(kernel,3,sizeof(int),&N);

    // 7️⃣ Kellotus + 1000 kertaa
    size_t globalSize[2] = {N,N};
    double t0 = current_time_ms();
    for(int i=0;i<1000;i++){
        err = clEnqueueNDRangeKernel(queue,kernel,2,NULL,globalSize,NULL,0,NULL,NULL);
        clFinish(queue);
    }
    double t1 = current_time_ms();
    printf("Executed 1000x OpenCL matrix multiplication in %.3f ms\n", t1-t0);

    // 8️⃣ Read result
    clEnqueueReadBuffer(queue, bufC, CL_TRUE, 0, sizeof(float)*N*N, C, 0,NULL,NULL);

    printf("Result matrix C:\n");
    for(int i=0;i<N;i++){
        for(int j=0;j<N;j++) printf("%.1f ", C[i*N + j]);
        printf("\n");
    }

    // Cleanup
    clReleaseKernel(kernel);
    clReleaseProgram(program);
    clReleaseMemObject(bufA);
    clReleaseMemObject(bufB);
    clReleaseMemObject(bufC);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);

    return 0;
}