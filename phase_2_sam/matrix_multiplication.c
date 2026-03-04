#include <stdio.h>
#include <stdlib.h>
#include <time.h>

double current_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec*1000.0 + ts.tv_nsec/1e6;
}

// Host CPU matrix multiplication
void cpu_matmul(float* A, float* B, float* C, int N) {
    for(int row=0; row<N; row++)
        for(int col=0; col<N; col++){
            float sum = 0.0f;
            for(int k=0; k<N; k++)
                sum += A[row*N + k] * B[k*N + col];
            C[row*N + col] = sum;
        }
}

int main() {
    const int N = 4;
    const int iterations = 1000;
    float A[16] = {1,2,3,4, 5,6,7,8, 9,10,11,12, 13,14,15,16};
    float B[16] = {16,15,14,13, 12,11,10,9, 8,7,6,5, 4,3,2,1};
    float C[16];

    printf("Running CPU matrix multiplication...\n");

    double t0 = current_time_ms();
    for(int i=0;i<iterations;i++){
        cpu_matmul(A,B,C,N);
    }
    double t1 = current_time_ms();

    printf("Executed %dx CPU matrix multiplication in %.3f ms\n", iterations, t1-t0);

    printf("Result matrix C:\n");
    for(int i=0;i<N;i++){
        for(int j=0;j<N;j++)
            printf("%.1f ", C[i*N + j]);
        printf("\n");
    }

    return 0;
}