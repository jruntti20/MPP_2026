#include <math.h>
#include "getAverage.h"
#include "getStandardDeviation.h"

float getStandardDeviation(unsigned char* inputImage, float avg, int u, int v, int n, int img_w, int d){
    
    float sum = 0;
    for (int i = -n; i <= n; i++){
        for (int j = -n; j <= n; j++){
            sum += pow((inputImage[(u + i) * img_w + (v + j - d)] - avg), 2);
        }
    }
    return sqrt(sum)/(2*n +1);
}
