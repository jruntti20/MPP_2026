#include "cross_checking.h"
#include <immintrin.h>

void cross_checking(unsigned char *outputImageLeftD, unsigned char * outputImageRightD, unsigned int img_h, unsigned int img_w, unsigned int threshold_val, int win_size) 
{
    const unsigned int scaled_h = img_h / 4;
    const unsigned int scaled_w = img_w / 4;
    const unsigned int border = (win_size - 1) / 2;
    const unsigned int simdWidth = 16;
    const __m128i zero = _mm_setzero_si128();
    const __m128i threshold = _mm_set1_epi8(static_cast<char>(threshold_val));

    for (unsigned int i = 0; i < scaled_h; i ++)
    {
        if (i < border || i >= scaled_h - border)
        {
            continue;
        }

        unsigned char* leftRow = outputImageLeftD + i * scaled_w;
        const unsigned char* rightRow = outputImageRightD + i * scaled_w;

        unsigned int j = border;
        const unsigned int simdEnd = (scaled_w > border) ? scaled_w - border : 0;

        for (; j + simdWidth <= simdEnd; j += simdWidth)
        {
            const __m128i leftPixels = _mm_loadu_si128(reinterpret_cast<const __m128i*>(leftRow + j));
            const __m128i rightPixels = _mm_loadu_si128(reinterpret_cast<const __m128i*>(rightRow + j));

            const __m128i diff = _mm_sub_epi8(_mm_max_epu8(leftPixels, rightPixels), _mm_min_epu8(leftPixels, rightPixels));
            const __m128i aboveThreshold = _mm_andnot_si128(_mm_cmpeq_epi8(_mm_subs_epu8(diff, threshold), zero), _mm_set1_epi8(static_cast<char>(0xFF)));
            const __m128i filtered = _mm_andnot_si128(aboveThreshold, leftPixels);

            _mm_storeu_si128(reinterpret_cast<__m128i*>(leftRow + j), filtered);
        }

        for (; j < scaled_w - border; j++)
        {
            if (abs(outputImageLeftD[j+i*(img_w/4)] - outputImageRightD[j+i*(img_w/4)]) > threshold_val){
                outputImageLeftD[j+i*(img_w/4)] = 0;
            }
        }
    }

}
