#include <errno.h>
#include <immintrin.h>
#include <stdio.h>

void resizeImage(const unsigned char* inputImage, unsigned char* resizedImg, int w, int h){

    if(errno) printf("Value of errno after calling function ResizeImage: %d\n", errno);

    int i,j;
    int new_w = w/4, new_h = h/4;

    if(errno) printf("Value of errno before looping: %d\n", errno);
    for (i = 0; i < new_h; i++) {
        for (j = 0; j < new_w; j++) {
            
            // Calculating corresponding indices in the original image
            int orig_i = (4*i-1*(i > 0)); 
            int orig_j = (4*j-1*(j > 0));

            // Resizing
            resizedImg[i*new_w+j] = inputImage[orig_i*w+orig_j];
        }
    }
    if(errno) printf(" Value of errno after looping: %d\n ", errno); 
}

void grayScaleImage(const unsigned char* inputImage, unsigned char* grayScaleImg, int w, int h){

    const __m128i rMask = _mm_setr_epi8(
        0, 4, 8, 12,
        -1, -1, -1, -1,
        -1, -1, -1, -1,
        -1, -1, -1, -1
    );
    const __m128i gMask = _mm_setr_epi8(
        1, 5, 9, 13,
        -1, -1, -1, -1,
        -1, -1, -1, -1,
        -1, -1, -1, -1
    );
    const __m128i bMask = _mm_setr_epi8(
        2, 6, 10, 14,
        -1, -1, -1, -1,
        -1, -1, -1, -1,
        -1, -1, -1, -1
    );
    const __m128 rCoeff = _mm_set1_ps(0.299f);
    const __m128 gCoeff = _mm_set1_ps(0.587f);
    const __m128 bCoeff = _mm_set1_ps(0.114f);
    const int simdWidth = 4;

    for (int i = 0; i < h; i++){
        const unsigned char* inputRow = inputImage + (i * w * 4);
        unsigned char* outputRow = grayScaleImg + (i * w);

        int j = 0;
        for (; j <= w - simdWidth; j += simdWidth){
            const __m128i rgba = _mm_loadu_si128(reinterpret_cast<const __m128i*>(inputRow + j * 4));

            const __m128 r = _mm_cvtepi32_ps(_mm_cvtepu8_epi32(_mm_shuffle_epi8(rgba, rMask)));
            const __m128 g = _mm_cvtepi32_ps(_mm_cvtepu8_epi32(_mm_shuffle_epi8(rgba, gMask)));
            const __m128 b = _mm_cvtepi32_ps(_mm_cvtepu8_epi32(_mm_shuffle_epi8(rgba, bMask)));

            __m128 gray = _mm_mul_ps(r, rCoeff);
            gray = _mm_add_ps(gray, _mm_mul_ps(g, gCoeff));
            gray = _mm_add_ps(gray, _mm_mul_ps(b, bCoeff));

            const __m128i gray32 = _mm_cvttps_epi32(gray);
            const __m128i gray16 = _mm_packus_epi32(gray32, _mm_setzero_si128());
            const __m128i gray8 = _mm_packus_epi16(gray16, _mm_setzero_si128());

            outputRow[j] = static_cast<unsigned char>(_mm_extract_epi8(gray8, 0));
            outputRow[j + 1] = static_cast<unsigned char>(_mm_extract_epi8(gray8, 1));
            outputRow[j + 2] = static_cast<unsigned char>(_mm_extract_epi8(gray8, 2));
            outputRow[j + 3] = static_cast<unsigned char>(_mm_extract_epi8(gray8, 3));
        }

        for (; j < w; j++){
            int idx = j * 4;
            outputRow[j] = static_cast<unsigned char>(
                0.299 * inputRow[idx] +
                0.587 * inputRow[idx + 1] +
                0.114 * inputRow[idx + 2]
            );
        }
    }
}

void grayScaleAndResizeImage(const unsigned char* inputImage, unsigned char* resizedImg, int w, int h){

    const int new_w = w / 4;
    const int new_h = h / 4;

    for (int i = 0; i < new_h; i++){
        const int orig_i = (4 * i - 1 * (i > 0));
        for (int j = 0; j < new_w; j++){
            const int orig_j = (4 * j - 1 * (j > 0));
            const int idx = (orig_i * w + orig_j) * 4;
            resizedImg[i * new_w + j] = static_cast<unsigned char>(
                0.299 * inputImage[idx] +
                0.587 * inputImage[idx + 1] +
                0.114 * inputImage[idx + 2]
            );
        }
    }
}
