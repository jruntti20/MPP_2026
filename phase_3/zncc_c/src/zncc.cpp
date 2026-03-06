#include <math.h>
#include <tuple>
#include <sys/time.h>
#include "getAverage.h"
#include "getStandardDeviation.h"
#include "zncc.h"
#include "stdio.h"

struct timeval loopBegin, loopEnd;

void zncc(unsigned char *inputImage1, unsigned char* inputImage2, unsigned char * outputImage, unsigned int u, unsigned int v, unsigned int img_w, unsigned int img_h, unsigned int win_size, int min_d, int max_d, unsigned long *loop_counter)
{
    // TODO implementoidaan disparity parametri ja palautetaan arvo, joka antaa parhaan ristikorrelaation.

    int n = (win_size/2);
    double best_zncc_score = 0;
    unsigned int best_disparity_val = 0;
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


        for (int i = -n; i <= n; i++){
            for (int j = -n; j <= n; j++){

                //Borders checking
                if (!(v+j-di>=0) || !(v+j-di<img_w)){
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
    //outputImage[u * img_w + v] = (float) best_disparity_val/abs(max_d - min_d)*255;
    outputImage[u * img_w + v] = best_disparity_val;
}
