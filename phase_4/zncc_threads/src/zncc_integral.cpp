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
            float best_zncc_score = 0;
            unsigned int best_disparity_val = 0;

            float avg1 = data->meanL[(y + r) * data->img_w + (x + r)];

            for (int d = 0; d < data->max_disp; d++)
            {
                
                float avg2 = data->meanR[(y + r) * data->img_w + (x + r - (d * data->disp_sign))]; 
                float zncc_score = 0;
                float left_pix_diff = 0;
                float right_pix_diff = 0;
                float upper_sum = 0;
                float lower_sum_0 = 0;
                float lower_sum_1 = 0;

                //float dot = 0.0f;
                for (int j = -r; j <= r; j++)
                {
                    for (int i = -r; i <= r; i++)
                    {

                        
                        //Borders checking
                        if (!(x+i-(d*data->disp_sign) >= 0) || !(x+i-(d * data->disp_sign) < data->img_w)){
                            continue;
                        }
                        


                        float a = data->left[(y+j)*data->img_w + x + i];
                        float b = data->right[(y+j)*data->img_w + x + i - (d * data->disp_sign)];

                        //dot += a*b;
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

            data->disparity[y*data->img_w + x] = (float)best_disparity_val/data->max_disp*255; 
        }
    }
}

void populate_integral_tables(unsigned char *inputImage1, unsigned char *inputImage2, uint32_t *inputIntegralL, uint32_t *inputIntegralR, uint32_t *inputIntegralSquaredL, uint32_t *inputIntegralSquaredR, float *meanTableL, float *meanTableR, float *varTableL, float *varTableR, int img_h, int img_w, int win_size)
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
                inputIntegralL[y*img_w + x] = row_sum1;
                inputIntegralR[y*img_w + x] = row_sum2;
                inputIntegralSquaredL[y*img_w + x] = row_sum1_squared;
                inputIntegralSquaredR[y*img_w + x] = row_sum2_squared;
            }
            else
            {
                inputIntegralL[y*img_w + x] = inputIntegralL[(y-1)*img_w + x] + row_sum1;
                inputIntegralR[y*img_w + x] = inputIntegralR[(y-1)*img_w + x] + row_sum2;
                inputIntegralSquaredL[y*img_w + x] = inputIntegralSquaredL[(y-1)*img_w + x] + row_sum1_squared;
                inputIntegralSquaredR[y*img_w + x] = inputIntegralSquaredR[(y-1)*img_w + x] + row_sum2_squared;
                
            }

        }
    }

    for (int y = n; y < img_h - n; y++)
        for (int x = n; x < img_w - n; x++)
        {
            int y_idx_min = y - n - 1;
            int x_idx_min = x - n - 1;
            int y_idx_max = y + n;
            int x_idx_max = x + n;

            uint32_t S0_0L = inputIntegralL[y_idx_min * img_w + x_idx_min];
            uint32_t S2_2L = inputIntegralL[y_idx_max * img_w + x_idx_max];
            uint32_t S2_0L = inputIntegralL[y_idx_max * img_w + x_idx_min];
            uint32_t S0_2L = inputIntegralL[y_idx_min * img_w + x_idx_max];
            uint32_t SQ0_0L = inputIntegralSquaredL[y_idx_min * img_w + x_idx_min];
            uint32_t SQ2_2L = inputIntegralSquaredL[y_idx_max * img_w + x_idx_max];
            uint32_t SQ2_0L = inputIntegralSquaredL[y_idx_max * img_w + x_idx_min];
            uint32_t SQ0_2L = inputIntegralSquaredL[y_idx_min * img_w + x_idx_max];

            uint32_t S0_0R = inputIntegralR[y_idx_min * img_w + x_idx_min];
            uint32_t S2_2R = inputIntegralR[y_idx_max * img_w + x_idx_max];
            uint32_t S2_0R = inputIntegralR[y_idx_max * img_w + x_idx_min];
            uint32_t S0_2R = inputIntegralR[y_idx_min * img_w + x_idx_max];
            uint32_t SQ0_0R = inputIntegralSquaredR[y_idx_min * img_w + x_idx_min];
            uint32_t SQ2_2R = inputIntegralSquaredR[y_idx_max * img_w + x_idx_max];
            uint32_t SQ2_0R = inputIntegralSquaredR[y_idx_max * img_w + x_idx_min];
            uint32_t SQ0_2R = inputIntegralSquaredR[y_idx_min * img_w + x_idx_max];

            if((y_idx_min < 0) && (x_idx_min < 0))
            {
                S0_0L = 0;
                SQ0_0L = 0;
                S0_0R = 0;
                SQ0_0R = 0;
            }
            else if((y_idx_min < 0) && !(x_idx_min < 0))
            {
                
            }

            if((y_idx_min < 0) || (x_idx_min < 0))
            {
                S0_0L = 0, S2_0L = 0,S0_2L = 0;
                SQ0_0L = 0, SQ2_0L = 0,SQ0_2L = 0;
                S0_0R = 0, S2_0R = 0,S0_2R = 0;
                SQ0_0R = 0, SQ2_0R = 0,SQ0_2R = 0;
            }
            meanTableL[y * img_w + x] = (S2_2L + S0_0L - S2_0L - S0_2L) / (float)win_size / (float)win_size;
            meanTableR[y * img_w + x] = (S2_2R + S0_0R - S2_0R - S0_2R)/ (float)win_size / (float)win_size;
            varTableL[y * img_w + x] = (SQ2_2L + SQ0_0L - SQ2_0L - SQ0_2L)/ (float)win_size / (float)win_size - meanTableL[y * img_w + x] * meanTableL[y * img_w + x];
            varTableR[y * img_w + x] = (SQ2_2R + SQ0_0R - SQ2_0R - SQ0_2R)/ (float)win_size / (float)win_size - meanTableR[y * img_w + x] * meanTableR[y * img_w + x];
    }
}


