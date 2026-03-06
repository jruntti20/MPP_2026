#include <CL/cl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#define N 100

const char *kernelSource =
"__kernel void add_matrix(__global float* A, __global float* B, __global float* C){"
"   int id = get_global_id(0);"
"   C[id] = A[id] + B[id];"
"}";

void add_matrix_cpu(float *A, float *B, float *C, int size)
{
    for(int i = 0; i < size; i++)
        C[i] = A[i] + B[i];
}

void print_platform_info()
{
    cl_platform_id platform;
    char buffer[1024];

    clGetPlatformIDs(1, &platform, NULL);

    clGetPlatformInfo(platform, CL_PLATFORM_NAME, sizeof(buffer), buffer, NULL);
    printf("Platform Name: %s\n", buffer);

    clGetPlatformInfo(platform, CL_PLATFORM_VENDOR, sizeof(buffer), buffer, NULL);
    printf("Vendor: %s\n", buffer);

    clGetPlatformInfo(platform, CL_PLATFORM_VERSION, sizeof(buffer), buffer, NULL);
    printf("Version: %s\n\n", buffer);
}

int main()
{
    int size = N * N;
    size_t bytes = size * sizeof(float);

    float *matrix_1 = (float*)malloc(bytes);
    float *matrix_2 = (float*)malloc(bytes);
    float *result_cpu = (float*)malloc(bytes);
    float *result_gpu = (float*)malloc(bytes);

    for(int i = 0; i < size; i++)
    {
        matrix_1[i] = i;
        matrix_2[i] = i * 2;
    }

    printf("---- OpenCL Platform Info ----\n");
    print_platform_info();

    struct timeval start, end;

    gettimeofday(&start, NULL);

    add_matrix_cpu(matrix_1, matrix_2, result_cpu, size);

    gettimeofday(&end, NULL);

    double cpu_time =
        (end.tv_sec - start.tv_sec) +
        (end.tv_usec - start.tv_usec)/1000000.0;

    printf("Host CPU execution time: %f seconds\n\n", cpu_time);

    cl_platform_id platform;
    cl_device_id device;
    cl_context context;
    cl_command_queue queue;
    cl_program program;
    cl_kernel kernel;

    cl_mem bufferA, bufferB, bufferC;

    cl_int err;

    err = clGetPlatformIDs(1, &platform, NULL);
    err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_DEFAULT, 1, &device, NULL);

    context = clCreateContext(NULL, 1, &device, NULL, NULL, &err);

    queue = clCreateCommandQueue(context, device, CL_QUEUE_PROFILING_ENABLE, &err);

    bufferA = clCreateBuffer(context, CL_MEM_READ_ONLY, bytes, NULL, NULL);
    bufferB = clCreateBuffer(context, CL_MEM_READ_ONLY, bytes, NULL, NULL);
    bufferC = clCreateBuffer(context, CL_MEM_WRITE_ONLY, bytes, NULL, NULL);

    cl_event write_eventA, write_eventB, kernel_event, read_event;

    clEnqueueWriteBuffer(queue, bufferA, CL_TRUE, 0, bytes, matrix_1, 0, NULL, &write_eventA);
    clEnqueueWriteBuffer(queue, bufferB, CL_TRUE, 0, bytes, matrix_2, 0, NULL, &write_eventB);

    program = clCreateProgramWithSource(context, 1, &kernelSource, NULL, &err);

    err = clBuildProgram(program, 1, &device, NULL, NULL, NULL);

    kernel = clCreateKernel(program, "add_matrix", &err);

    clSetKernelArg(kernel, 0, sizeof(cl_mem), &bufferA);
    clSetKernelArg(kernel, 1, sizeof(cl_mem), &bufferB);
    clSetKernelArg(kernel, 2, sizeof(cl_mem), &bufferC);

    size_t globalSize = size;

    clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &globalSize, NULL, 0, NULL, &kernel_event);

    clFinish(queue);

    clEnqueueReadBuffer(queue, bufferC, CL_TRUE, 0, bytes, result_gpu, 0, NULL, &read_event);

    cl_ulong start_kernel, end_kernel;

    clGetEventProfilingInfo(kernel_event, CL_PROFILING_COMMAND_START,
                            sizeof(cl_ulong), &start_kernel, NULL);

    clGetEventProfilingInfo(kernel_event, CL_PROFILING_COMMAND_END,
                            sizeof(cl_ulong), &end_kernel, NULL);

    double kernel_time = (end_kernel - start_kernel) / 1000000000.0;

    printf("Device kernel execution time: %f seconds\n", kernel_time);

    printf("\nFirst 5 results (GPU):\n");
    for(int i=0;i<5;i++)
        printf("%f ", result_gpu[i]);

    printf("\n");

    clReleaseMemObject(bufferA);
    clReleaseMemObject(bufferB);
    clReleaseMemObject(bufferC);
    clReleaseKernel(kernel);
    clReleaseProgram(program);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);

    free(matrix_1);
    free(matrix_2);
    free(result_cpu);
    free(result_gpu);

    return 0;
}