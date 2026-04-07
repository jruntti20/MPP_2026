#include "getAverage.h"
#include <immintrin.h>
#include "zncc_integral.h"

namespace
{
// Reduce four 32-bit lane sums in one SIMD register into one scalar float.
// After the SIMD dot-product loop, each lane contains a partial sum.
inline float horizontal_sum_epi32(__m128i value)
{
    value = _mm_hadd_epi32(value, value);
    value = _mm_hadd_epi32(value, value);
    return static_cast<float>(_mm_cvtsi128_si32(value));
}
}

template <typename T> T get_window_sum(T *integralImage, int img_w, int x, int y, int r)
{
    T A = integralImage[(y - r - 1) * img_w + (x - r - 1)];
    T B = integralImage[(y - r - 1) * img_w + (x + r)];
    T C = integralImage[(y + r) * img_w + (x - r - 1)];
    T D = integralImage[(y + r) * img_w + (x + r)];

    return D - B - C + A;
}

void zncc_worker(ThreadData *data)
{
    int r = data->window_size / 2;
    int N = data->window_size * data->window_size;
    // One 128-bit register holds 16 unsigned char pixels.
    const int simdWidth = 16;
    const __m128i zero = _mm_setzero_si128();

    for(int y = data->y_start; y < data->y_end; y++)
    {
        if (y == 0 || y == data->img_h - 1)
        {
            continue; // Exclude padded edges
        }
        for(int x = r; x < data->img_w - r; x++)
        {
            // Skip the padded border. The integral-image based variance/mean tables
            // and window accesses assume a fully valid neighborhood around (x, y).
            if (x - r == 0 || x + r == data->img_w - 1)
            {
                continue; // Exclude padded edges
            }
            float best_zncc_score = -1.0f;
            unsigned int best_disparity_val = 0;

            float meanA = data->meanL[y * data->img_w + x];
            float varA = data->varL[y * data->img_w + x];
            // Flat windows are not informative for correlation.
            if(varA < 1e-6f) continue;

            for (int d = 0; d < data->max_disp; d++)
            {
                // Compare left pixel x against right pixel xr for disparity d.
                int xr = x - d * data->disp_sign;
                if (xr - r < 0 || xr + r >= data->img_w)
                {
                    continue;
                }

                float meanB = data->meanR[y * data->img_w + xr];
                float varB = data->varR[y * data->img_w + xr];
                if(varB < 1e-6f) continue;

                // Final dot product for this (x, y, d):
                // sum over the window of leftPixel * rightPixel.
                // We accumulate part of it in SIMD lanes and the remainder in scalar.
                float dot = 0.0f;
                __m128i dotAccum = _mm_setzero_si128();

                for (int j = -r; j <= r; j++)
                {
                    // Start of the current row inside the correlation window.
                    const unsigned char* leftRow = data->left + (y + j) * data->img_w + (x - r);
                    const unsigned char* rightRow = data->right + (y + j) * data->img_w + (xr - r);
                    int i = 0;

                    for (; i <= data->window_size - simdWidth; i += simdWidth)
                    {
                        // Load 16 grayscale pixels from both windows.
                        const __m128i leftPixels = _mm_loadu_si128(reinterpret_cast<const __m128i*>(leftRow + i));
                        const __m128i rightPixels = _mm_loadu_si128(reinterpret_cast<const __m128i*>(rightRow + i));

                        // Expand 8-bit pixels into 16-bit values so the products fit safely.
                        // Low and high halves are expanded separately.
                        const __m128i leftLo = _mm_unpacklo_epi8(leftPixels, zero);
                        const __m128i leftHi = _mm_unpackhi_epi8(leftPixels, zero);
                        const __m128i rightLo = _mm_unpacklo_epi8(rightPixels, zero);
                        const __m128i rightHi = _mm_unpackhi_epi8(rightPixels, zero);

                        // _mm_madd_epi16 multiplies adjacent 16-bit pairs and adds them
                        // into 32-bit lanes:
                        // lane0 = a0*b0 + a1*b1, lane1 = a2*b2 + a3*b3, ...
                        // This is the core SIMD speedup for the dot product.
                        dotAccum = _mm_add_epi32(dotAccum, _mm_madd_epi16(leftLo, rightLo));
                        dotAccum = _mm_add_epi32(dotAccum, _mm_madd_epi16(leftHi, rightHi));
                    }

                    // Handle the tail when window_size is not a multiple of 16.
                    for (; i < data->window_size; i++)
                    {
                        dot += static_cast<float>(leftRow[i]) * static_cast<float>(rightRow[i]);
                    }
                }

                // Collapse the four SIMD partial sums into one scalar and add it to dot.
                dot += horizontal_sum_epi32(dotAccum);

                // With means and variances already precomputed from integral tables,
                // only the numerator dot term needs to be accumulated here.
                float numerator = dot - N * meanA * meanB;
                float denom = sqrtf(varA * varB) * N + 1e-6f;

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

    // Build summed-area tables for pixel values and squared pixel values.
    // These tables let us query any window sum in O(1), which avoids recomputing
    // means and variances from scratch for every disparity candidate.
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

    // Convert integral-image sums into per-pixel mean and variance tables.
    // These are later reused by every thread in zncc_worker.
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
