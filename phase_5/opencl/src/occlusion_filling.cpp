#include "occlusion_filling.h"

void occlusion_filling_nearest_pixel(unsigned char *readyOutputImg, unsigned char *outputImageLeftD, int img_h, int img_w, int range, int win_size)
{

    unsigned int pixval;
    int left, right, top, bottom, x, y, rp, rn;
    for (int i = 0; i < img_h; i++){
        for (int j = 0; j < img_w; j++){
            if (((int) i - (win_size -1)/2 < 0) || ((int) i + (win_size -1)/2 >= (int)img_h || ((int) j - (win_size -1)/2 < 0) || ((int) j + (win_size -1)/2 >= (int)img_w)))
                    {
                continue;
            }
            if (outputImageLeftD[j + i*img_w] == 0)
            {
                for (rp = 0, rn = 0; rp < range && rn > -range; rp++, rn--)
                {
                    left = j + rn;
                    right = j + rp;
                    top = i + rn;
                    bottom = i + rp;
                    if(left<0){left = 0;}
                    if(right>=img_w){right = img_w - 1;}
                    if(top < 0){top = 0;}
                    if(bottom>=img_h){bottom = img_h - 1;}
                        
                    pixval = 0;

                    for (x=left; x<=right; x++)
                    {
                        for (y = top; y <= bottom; y++)
                        {
                            if (outputImageLeftD[x + y*img_w] > pixval)
                            {
                                pixval = outputImageLeftD[x + y*img_w];
                            }
                        }
                    }
                    if(pixval != 0){
                        readyOutputImg[j + i*img_w] = pixval;
                        break;
                    }
                }
            } else {
                readyOutputImg[j + i * img_w] = outputImageLeftD[j + i * img_w];
            }
        }
    }
}

void occlusion_filling_lininterp(unsigned char *readyOutputImg, unsigned char *outputImageLeftD, unsigned int img_h, unsigned int img_w, int win_size)  
{
    for(unsigned int y = (win_size -1) / 2 - 1; y < img_h - (win_size - 1)/ 2; y++)
    {
        unsigned int x = (win_size -1) / 2 - 1;

        while (x < (img_w - (win_size - 1) / 2))
        {
            int idx = y * img_w + x;
            if (outputImageLeftD[idx] > 0)
            {
                x++;
                readyOutputImg[y * img_w + x] = outputImageLeftD[y * img_w + x];
                continue;
            }
            int start = x;

            while((x < (img_w - (win_size - 1) / 2)) && (outputImageLeftD[y * img_w + x] == 0))
            {
                x++;
            }
            unsigned int end = x - 1;

            int leftIdx = start - 1;
            int rightIdx = x;

            int leftVal = (leftIdx > 0) ? outputImageLeftD[y * img_w + leftIdx] : 0;
            int rightVal = (rightIdx < ((int)img_w - (win_size - 1) / 2)) ? outputImageLeftD[y * img_w + rightIdx] : 0;

            int hole_size = end - start + 1;

            for (int i = 0; i < hole_size; i++)
            {
                int px = start + i;
                int pidx = y * img_w + px;
                if (leftVal != 0 && rightVal != 0)
                {
                    float t = (float)(i + 1) / (hole_size + 1);
                    readyOutputImg[pidx] = (int)((1.0f - t) * leftVal + t * rightVal);
                }
                else if (leftVal != 0)
                {
                    readyOutputImg[pidx] = leftVal;
                }
                else if(rightVal != 0)
                {
                    readyOutputImg[pidx] = rightVal;
                }
            }
        }
    }
    fillSinglePixelsMedian(readyOutputImg, (int)img_w, (int)img_h, win_size);
}

void fillSinglePixels(unsigned char *readyOutputImg, int img_w, int img_h, int window_size)
{
    for (int y = (window_size -1) / 2 - 1; y < (img_h - (window_size - 1) / 2); y++)
    {
        for (int x = (window_size - 1) / 2 ; x < (img_w < (window_size - 1) / 2); x++)
        {
            int idx = y * img_w + x;
            if (readyOutputImg[idx] == 0)
            {
                float left = readyOutputImg[y * img_w + (x -1)];
                float right = readyOutputImg[y * img_w + (x + 1)];

                if(left > 0 && right > 0)
                {readyOutputImg[idx] = 0.5f * (left + right);}
                else if (left > 0){readyOutputImg[idx] = left;}
                else if (right > 0){readyOutputImg[idx] = right;}
            }
        }
    }
}

void fillSinglePixelsMedian(unsigned char *readyOutputImg, int img_w, int img_h, int window_size)
{

    int filter_window = 5;

    int start_y = (window_size - 1) / 2 - 1;
    int end_y = img_h - (window_size - 1) / 2 - 1;
    int start_x = start_y;
    int end_x = img_w - (window_size - 1) / 2 - 1;

    for (int y = start_y; y < end_y; y++)
    {
        for (int x = start_x; x < end_x; x++)
        {
            if ((y - (filter_window - 1) / 2 < start_y) || (y + (filter_window - 1) / 2 >= end_y)
                    || (x - (filter_window - 1) / 2 < start_x) || (x + (filter_window - 1) / 2 >= end_x))
                    {
                        continue;
                    }


            int idx = y * img_w + x;
            if (readyOutputImg[idx] == 0)
            {
                
                int sum = 0;
                int count = 0;

                int n = (filter_window - 1) / 2;

                for (int i = -n; i <= n; i++)
                {
                    for (int j = -n; j <= n; j++)
                    {
                        float val = readyOutputImg[(y + i) * img_w + (x + j)];
                        if (val != 0)
                        {
                            sum += val;
                            count++;
                        }
                    }
                }

                readyOutputImg[idx] = sum / count;
            }
        }
    }
}
void normalize_disparity_map(unsigned char *readyOutputImg, int img_h, int img_w, int max_disp, int min_disp)
{
    for (int y = 0; y < img_h; y++)
    {
        for(int x = 0; x < img_w; x++)
        {
           readyOutputImg[y * img_w + x] = (float)readyOutputImg[y * img_w + x]/abs(max_disp - min_disp)*255;
        }
    }

}

