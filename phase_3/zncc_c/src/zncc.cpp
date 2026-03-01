#include <math.h>
#include <tuple>
#include <sys/time.h>
#include "getAverage.h"
#include "getStandardDeviation.h"
#include "zncc.h"
#include "stdio.h"

struct timeval loopBegin, loopEnd;

void zncc(unsigned char *inputImage1, unsigned char* inputImage2, unsigned char * outputImage, unsigned int u, unsigned int v, unsigned int img_w, unsigned int img_h, unsigned int win_size, int min_d, int max_d, unsigned long *loop_counter){
    // TODO implementoidaan disparity parametri ja palautetaan arvo, joka antaa parhaan ristikorrelaation.

    int n = (win_size/2);
    double best_zncc_score = 0;
    unsigned int best_disparity_val = 0;
    //gettimeofday(&loopBegin, NULL);
    //double sdev1 = getStandardDeviation(inputImage1, avg1, u, v, n, img_w, 0);
    float avg1 = getAverage(inputImage1, u, v, n, img_w, loop_counter);
    for (int di = min_d; di <= max_d; di++){
        
        if (di >= 0){
            if (!(v - di >= 0) || !(v < img_w)){
                continue;
            }
        } else {
            if (!(v >=0 ) || !(v - di < img_w)){
                continue;
            }
        }
        float avg2 = getAverage(inputImage2, u, v-di, n, img_w, loop_counter);
        float zncc_score = 0;
        float left_pix_diff = 0;
        float right_pix_diff = 0;
        float upper_sum = 0;
        float lower_sum_0 = 0;
        float lower_sum_1 = 0;

        //double sdev2 = getStandardDeviation(inputImage2, avg2, u, v, n, img_w, di);

        for (int i = -n; i <= n; i++){
            for (int j = -n; j <= n; j++){

                //TODO: Borders checking
                //if (!(u+i>=0) || !(u+i<img_h) || !(v+j>=0) || !(v+j<img_w) || !(v+j-di>=0) || !(v+j-di<img_w)){
                if (!(v+j-di>=0) || !(v+j-di<img_w)){
                //if (!(u+i>=0) || !(u+i<img_h) || !(v+j>=0) || !(v+j<img_w)){
                    //printf("img_h: %d, img_w: %d\n", img_h, img_w);
                    //printf("u: %d, i: %d, v: %d, j: %d, di: %d\n", u, i, v, j, di); 
                    //printf("u+i = %d\n", u+i);
                    //printf("v+j-di = %d\n", v+j-di);
                    continue;
                }
                left_pix_diff = (inputImage1[(u + i) * img_w + (v + j)] - avg1);
                right_pix_diff = (inputImage2[(u + i) * img_w + (v + j - di)] - avg2);
                upper_sum +=  left_pix_diff * right_pix_diff;
                lower_sum_0 += pow(left_pix_diff,2);
                lower_sum_1 += pow(right_pix_diff, 2);
                (*loop_counter)++;
            }

        }
        zncc_score = upper_sum / (sqrt(lower_sum_0) * sqrt(lower_sum_1));
        if (zncc_score > best_zncc_score){
            best_zncc_score = zncc_score;
            best_disparity_val = abs(di);
        }

    }
    //gettimeofday(&loopEnd, NULL);
    //printf("One loop iteration: %ld microseconds\n", (loopEnd.tv_sec -loopBegin.tv_sec)*1000000L + (loopEnd.tv_usec - loopBegin.tv_usec));
    outputImage[u * img_w + v] = (float) best_disparity_val/abs(max_d - min_d)*255;
}
