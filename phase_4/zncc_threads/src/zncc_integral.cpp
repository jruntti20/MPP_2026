#include "getAverage.h"
#include "zncc_integral.h"

void zncc_worker(ThreadData *data)
{
    //int r = data->window_size;
    //int N = (2*r+1)*(2*r+1);
    int r = data->window_size / 2 - 1;
    int N = data->window_size * data->window_size;;

    for(int y = data->y_start; y < data->y_end; y++)
    {
        for(int x = r; x < data->img_w - r; x++)
        {
            float best_zncc_score = -1.0f;
            int best_disparity_val = 0;

            for (int d = 0; d < data->max_disp; d++)
            {
                float dot = 0.0f;
                for (int j = -r; j <= r; j++)
                {
                    for (int i = -r; i <= r; i++)
                    {
                        float a = data->left[(y+j)*data->img_w + x + i];
                        float b = data->right[(y+j)*data->img_w + x + i - (d * data->disp_sign)];

                        dot += a*b;
                    }
                }

                float meanA = data->meanL[y*data->img_w + x];
                float meanB = data->meanR[y*data->img_w + x - (d * data->disp_sign)];

                float varA = data->varL[y*data->img_w + x];
                float varB = data->varR[y*data->img_w + x - (d * data->disp_sign)];

                float numerator = dot - N * meanA * meanB;
                float denom = sqrt(N * varA * N * varB);

                float zncc = numerator / denom;

                if(zncc > best_zncc_score)
                {
                    best_zncc_score = zncc;
                    best_disparity_val = d;
                }

            }

            data->disparity[y*data->img_w + x] = best_disparity_val; 
        }
    }
}

void populate_integral_tables(unsigned char *inputImage1, unsigned char *inputImage2, uint32_t *input1Integral, uint32_t *input2Integral, uint32_t *input1IntegralSquared, uint32_t *input2IntegralSquared, uint32_t *meanTable1, uint32_t *meanTable2, uint32_t *varTable1, uint32_t *varTable2, int img_h, int img_w, int win_size)
{
    int n = (win_size/2);
    double best_zncc_score = 0;

    for (int y = 0; y < img_h; y++)
    {
        unsigned int row_sum1 = 0;
        unsigned int row_sum2 = 0;
        unsigned int row_sum1_squared = 0;
        unsigned int row_sum2_squared = 0;
        for (int x = 0; x < img_w; x++)
        {
            row_sum1 += inputImage1[y * img_w + x];
            row_sum2 += inputImage2[y * img_w + x];
            row_sum1_squared += pow(inputImage1[y * img_w + x],2);
            row_sum2_squared += pow(inputImage2[y * img_w + x],2);
            if(y==0)
            {
                input1Integral[y*img_w + x] = row_sum1;
                input2Integral[y*img_w + x] = row_sum2;
                input1IntegralSquared[y*img_w + x] = row_sum1_squared;
                input2IntegralSquared[y*img_w + x] = row_sum2_squared;
            }
            else
            {
                input1Integral[y*img_w + x] = input1Integral[(y-1)*img_w + x] + row_sum1;
                input2Integral[y*img_w + x] = input2Integral[(y-1)*img_w + x] + row_sum2;
                input1IntegralSquared[y*img_w + x] = input1IntegralSquared[(y-1)*img_w + x] + row_sum1_squared;
                input2IntegralSquared[y*img_w + x] = input2IntegralSquared[(y-1)*img_w + x] + row_sum2_squared;
                
            }

        }
    }

    for (int y = n; y < img_h - n; y++)
        for (int x = n; x < img_w - n; x++)
        {
            meanTable1[y * img_w + x] = (input1Integral[(y - n) * img_w + (x - n)] + input1Integral[y * img_w + x] - input1Integral[y * img_w + (x - n)] - input1Integral[(y - n) * img_w + x]) / win_size / win_size;
            meanTable2[y * img_w + x] = (input2Integral[(y - n) * img_w + (x - n)] + input2Integral[y * img_w + x] - input2Integral[y * img_w + (x - n)] - input2Integral[(y - n) * img_w + x]) / win_size / win_size;
            varTable1[y * img_w + x] = (input1IntegralSquared[(y - n) * img_w + (x - n)] + input1IntegralSquared[y * img_w + x] - input1IntegralSquared[y * img_w + (x - n)] - input1IntegralSquared[(y - n) * img_w + x]) / win_size / win_size - meanTable1[y * img_w + x] * meanTable1[y * img_w + x];
            varTable2[y * img_w + x] = (input2IntegralSquared[(y - n) * img_w + (x - n)] + input2IntegralSquared[y * img_w + x] - input2IntegralSquared[y * img_w + (x - n)] - input2IntegralSquared[(y - n) * img_w + x]) / win_size / win_size - meanTable2[y * img_w + x] * meanTable2[y * img_w + x];
    }
}


