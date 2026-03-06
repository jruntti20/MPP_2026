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
    const int N = 100;
    const int iterations = 1000;
    size_t bytes = N*N*sizeof(float);

    // Allocate matrices dynamically
    float *A = (float*)malloc(bytes);
    float *B = (float*)malloc(bytes);
    float *C = (float*)malloc(bytes);

    for(int i=0;i<N*N;i++){
        A[i] = i % 100 + 1;
        B[i] = (i % 100 + 1)*2;
    }

    //printf("Running CPU matrix multiplication...\n");
    printf("Running 1000 iterations of **CPU** matrix multiplication on 100x100 matrices...\n");
    double t0 = current_time_ms();
    for(int i=0;i<iterations;i++){
        cpu_matmul(A,B,C,N);
    }
    double t1 = current_time_ms();

    printf("Executed %dx CPU matrix multiplication in %.3f ms\n", iterations, t1-t0);

    printf("C[0..4]: ");
    for(int i=0;i<5;i++) printf("%.1f ", C[i]);
    printf("\n");

    free(A);
    free(B);
    free(C);

    return 0;
}