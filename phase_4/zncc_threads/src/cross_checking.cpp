#include "cross_checking.h"

void cross_checking(unsigned char *outputImageLeftD, unsigned char * outputImageRightD, unsigned int img_h, unsigned int img_w, unsigned int threshold_val, int win_size) 
{

    unsigned int h = img_h / 4;
    unsigned int w = img_w / 4;
    int half = (win_size - 1) / 2;

    #pragma omp parallel for collapse(2) schedule(static)
    for (int i = 0; i < h; i ++)
    {
        for (int j = 0; j < w; j++)
        {
            if ((i - half < 0) || (i + half >= (int)h) || (j - half < 0) || (j + half >= (int)w)){
                continue;
            }
            if (abs(outputImageLeftD[j+i*w] - outputImageRightD[j+i*w]) > threshold_val){
                outputImageLeftD[j+i*w] = 0;
            }
        }
    }

}
