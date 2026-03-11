#include "cross_checking.h"

void cross_checking(unsigned char *outputImageLeftD, unsigned char * outputImageRightD, unsigned int img_h, unsigned int img_w, unsigned int threshold_val, int win_size) 
{
    for (unsigned int i = 0; i < img_h/4; i ++)
    {
        for (unsigned int j = 0; j < img_w/4; j++)
        {
            if (((int) i - (win_size -1)/2 < 0) || ((int) i + (win_size -1)/2 >= (int)img_h/4) || ((int) j - (win_size -1)/2 < 0) || ((int) j + (win_size -1)/2 >= (int)img_w/4)){
                continue;
            }
            if (abs(outputImageLeftD[j+i*(img_w/4)] - outputImageRightD[j+i*(img_w/4)]) > threshold_val){
                outputImageLeftD[j+i*(img_w/4)] = 0;
            }
        }
    }

}
