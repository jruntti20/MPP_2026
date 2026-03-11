#include <math.h>
#include "getAverage.h"

float getAverage(unsigned char* inputImage, int u, int v, int n, int img_w, unsigned long * loop_counter){
    
    float sum = 0;
    for (int i = -n; i <= n; i++){
        for (int j = -n; j <= n; j++){

            sum += inputImage[(u + i) * img_w + (v + j)];
        }
    }
    return sum/pow((2*n+1),2);

}
