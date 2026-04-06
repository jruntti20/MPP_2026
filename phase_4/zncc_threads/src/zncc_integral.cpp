#include "zncc_integral.h"

template <typename T> T get_window_sum(T *integralImage, int img_w, int x, int y, int r)
{
    T A = integralImage[(y - r - 1) * img_w + (x - r - 1)];
    T B = integralImage[(y - r - 1) * img_w + (x + r)];
    T C = integralImage[(y + r) * img_w + (x - r - 1)];
    T D = integralImage[(y + r) * img_w + (x + r)];

    return D - B - C + A;
}

void zncc_omp(OMPData* data)
    {

    int r = (data->window_size - 1) / 2;
    int N = data->window_size * data->window_size;

    #pragma omp parallel for
    for (int y = 0; y < data->img_h; y++)
        {
        if (y == 0 || y == data->img_h - 1)
            {
            continue; // Exclude padded edges
            }
        #pragma omp parallel for schedule(dynamic, 16)
        for (int x = 0; x < data->img_w; x++)
            {
            if (x - r == 0 || x + r == data->img_w - 1)
                {
                continue; // Exclude padded edges
                }
            float best_zncc_score = -1.0f;
            unsigned int best_disparity_val = 0;

            float meanA = data->meanL[y * data->img_w + x];
            float varA = data->varL[y * data->img_w + x];
            if (varA < 1e-6f) continue;

            for (int d = 0; d < data->max_disp; d++)
                {
                int xr = x - d * data->disp_sign;
                if (xr - r < 0 || xr + r >= data->img_w)
                    {
                    continue;
                    }

                float meanB = data->meanR[y * data->img_w + xr];
                float varB = data->varR[y * data->img_w + xr];
                if (varB < 1e-6f) continue;

                float dot = 0.0f;

                for (int j = -r; j <= r; j++)
                    {
                    for (int i = -r; i <= r; i++)
                        {

                        float a = data->left[(y + j) * data->img_w + x + i];
                        float b = data->right[(y + j) * data->img_w + xr + i];

                        dot += a * b;
                        }
                    }

                float numerator = dot - N * meanA * meanB;
                float denom = sqrt(varA * varB) * N + 1e-6f;

                float zncc = numerator / denom;

                if (zncc > best_zncc_score)
                    {
                    best_zncc_score = zncc;
                    best_disparity_val = d;
                    }

                }

            data->disparity[y * data->img_w + x] = best_disparity_val;

            }

        }
    }

void zncc_worker(ThreadData *data)
{
    int r = data->window_size / 2;
    int N = data->window_size * data->window_size;

    for(int y = data->y_start; y < data->y_end; y++)
    {
        if (y == 0 || y == data->img_h - 1)
        {
            continue; // Exclude padded edges
        }
        for(int x = r; x < data->img_w - r; x++)
        {
            if (x - r == 0 || x + r == data->img_w - 1)
            {
                continue; // Exclude padded edges
            }
            float best_zncc_score = -1.0f;
            unsigned int best_disparity_val = 0;

            float meanA = data->meanL[y * data->img_w + x];
            float varA = data->varL[y * data->img_w + x];
            if(varA < 1e-6f) continue;

            for (int d = 0; d < data->max_disp; d++)
            {
                int xr = x - d * data->disp_sign;
                if (xr - r < 0 || xr + r >= data->img_w)
                {
                    continue;
                }

                float meanB = data->meanR[y * data->img_w + xr];
                float varB = data->varR[y * data->img_w + xr];
                if(varB < 1e-6f) continue;

                float dot = 0.0f;

                for (int j = -r; j <= r; j++)
                {
                    for (int i = -r; i <= r; i++)
                    {

                        float a = data->left[(y+j)*data->img_w + x + i];
                        float b = data->right[(y+j)*data->img_w + xr + i];

                        dot += a*b;
                    }
                }

                float numerator = dot - N * meanA * meanB;
                float denom = sqrt(varA * varB) * N + 1e-6f;

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

void populate_integral_tables(unsigned char *inputImage1, unsigned char *inputImage2, uint32_t *inputIntegralL, uint32_t *inputIntegralR, uint64_t *inputIntegralSquaredL, uint64_t *inputIntegralSquaredR, float *meanTableL, float *meanTableR, float *varTableL, float *varTableR, int img_h, int img_w, int win_size)
{
    int n = (win_size/2);
    #pragma omp parallel for 
    for (int y = 0; y < img_h; y++)
    {
        unsigned int row_sum1 = 0;
        unsigned int row_sum2 = 0;
        unsigned int row_sum1_squared = 0;
        unsigned int row_sum2_squared = 0;
        #pragma omp parallel for reduction(+:row_sum1, row_sum2, row_sum1_squared, row_sum2_squared)
        for (int x = 0; x < img_w; x++)
        {
            row_sum1 += inputImage1[y * img_w + x];
            row_sum2 += inputImage2[y * img_w + x];
            row_sum1_squared += (inputImage1[y * img_w + x] * inputImage1[y * img_w + x]) ;
            row_sum2_squared += (inputImage2[y * img_w + x] * inputImage2[y * img_w + x]);
            
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
    #pragma omp parallel for collapse(2)
    for (int y = n + 1; y < img_h - n - 1; y++)
        for (int x = n + 1; x < img_w - n - 1; x++)
        {
            int y_idx_min = y - n - 1;
            int x_idx_min = x - n - 1;
            int y_idx_max = y + n;
            int x_idx_max = x + n;

            uint32_t S0_0L = inputIntegralL[y_idx_min * img_w + x_idx_min];
            uint32_t S2_2L = inputIntegralL[y_idx_max * img_w + x_idx_max];
            uint32_t S2_0L = inputIntegralL[y_idx_max * img_w + x_idx_min];
            uint32_t S0_2L = inputIntegralL[y_idx_min * img_w + x_idx_max];
            uint64_t SQ0_0L = inputIntegralSquaredL[y_idx_min * img_w + x_idx_min];
            uint64_t SQ2_2L = inputIntegralSquaredL[y_idx_max * img_w + x_idx_max];
            uint64_t SQ2_0L = inputIntegralSquaredL[y_idx_max * img_w + x_idx_min];
            uint64_t SQ0_2L = inputIntegralSquaredL[y_idx_min * img_w + x_idx_max];

            uint32_t S0_0R = inputIntegralR[y_idx_min * img_w + x_idx_min];
            uint32_t S2_2R = inputIntegralR[y_idx_max * img_w + x_idx_max];
            uint32_t S2_0R = inputIntegralR[y_idx_max * img_w + x_idx_min];
            uint32_t S0_2R = inputIntegralR[y_idx_min * img_w + x_idx_max];
            uint64_t SQ0_0R = inputIntegralSquaredR[y_idx_min * img_w + x_idx_min];
            uint64_t SQ2_2R = inputIntegralSquaredR[y_idx_max * img_w + x_idx_max];
            uint64_t SQ2_0R = inputIntegralSquaredR[y_idx_max * img_w + x_idx_min];
            uint64_t SQ0_2R = inputIntegralSquaredR[y_idx_min * img_w + x_idx_max];

            meanTableL[y * img_w + x] = (S2_2L + S0_0L - S2_0L - S0_2L) / (float)win_size / (float)win_size;
            meanTableR[y * img_w + x] = (S2_2R + S0_0R - S2_0R - S0_2R)/ (float)win_size / (float)win_size;

            varTableL[y * img_w + x] = (SQ2_2L + SQ0_0L - SQ2_0L - SQ0_2L)/ (float)win_size / (float)win_size - meanTableL[y * img_w + x] * meanTableL[y * img_w + x];
            varTableR[y * img_w + x] = (SQ2_2R + SQ0_0R - SQ2_0R - SQ0_2R)/ (float)win_size / (float)win_size - meanTableR[y * img_w + x] * meanTableR[y * img_w + x];
    }
}


