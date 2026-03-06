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
    const int N = 100;
    const int iterations = 1000;
    size_t bytes = N*N*sizeof(float);

    // Allocate matrices dynamically
    float *A = (float*)malloc(bytes);
    float *B = (float*)malloc(bytes);
    float *C = (float*)malloc(bytes);

    // Initialize matrices
    for(int i=0;i<N*N;i++){
        A[i] = i % 100 + 1;  // 1..100 repeating
        B[i] = (i % 100 + 1)*2; // 2,4,6,...
    }

    // 1️⃣ Platform
    cl_platform_id platform;
    err = clGetPlatformIDs(1,&platform,NULL);
    CHECK_ERROR(err,"Failed to get platform");

    // 2️⃣ Device
    cl_device_id device;
    err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_CPU, 1, &device, nullptr);
    CHECK_ERROR(err,"Failed to get device");

    char deviceName[128];
    clGetDeviceInfo(device, deviceName, sizeof(deviceName), deviceName, NULL);
    printf("Running 1000 iterations of **GPU** matrix multiplication on 100x100 matrices...\n");

    // 3️⃣ Context & Queue
    cl_context context = clCreateContext(NULL,1,&device,NULL,NULL,&err);
    CHECK_ERROR(err,"Failed to create context");
    cl_command_queue queue = clCreateCommandQueue(context,device,CL_QUEUE_PROFILING_ENABLE,&err);
    CHECK_ERROR(err,"Failed to create queue");

    // 4️⃣ Buffers
    cl_mem bufA = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, bytes, A, &err);
    cl_mem bufB = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, bytes, B, &err);
    cl_mem bufC = clCreateBuffer(context, CL_MEM_WRITE_ONLY, bytes, NULL, &err);

    // 5️⃣ Program & Kernel
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

    // 6️⃣ Run kernel
    size_t globalSize[2] = {N,N};
    double t0 = current_time_ms();
    for(int i=0;i<iterations;i++){
        clEnqueueNDRangeKernel(queue,kernel,2,NULL,globalSize,NULL,0,NULL,NULL);
        clFinish(queue);
    }
    double t1 = current_time_ms();
    printf("Executed %dx OpenCL matrix multiplication in %.3f ms\n", iterations, t1-t0);

    // 7️⃣ Read result
    clEnqueueReadBuffer(queue,bufC,CL_TRUE,0,bytes,C,0,NULL,NULL);

    printf("C[0..4]: ");
    for(int i=0;i<5;i++) printf("%.1f ", C[i]);
    printf("\n");

    // Cleanup
    clReleaseKernel(kernel);
    clReleaseProgram(program);
    clReleaseMemObject(bufA);
    clReleaseMemObject(bufB);
    clReleaseMemObject(bufC);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);

    free(A);
    free(B);
    free(C);

    return 0;
}