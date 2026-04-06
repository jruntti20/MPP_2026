#include <errno.h>
#include <stdio.h>
#include <omp.h>



void resizeImage(const unsigned char* inputImage, unsigned char* resizedImg, int w, int h){

    if(errno) printf("Value of errno after calling function ResizeImage: %d\n", errno);

    int i,j;
    int new_w = w/4, new_h = h/4;

    if(errno) printf("Value of errno before looping: %d\n", errno);
    #pragma omp parallel for collapse(2)
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

    int i,j;
    #pragma omp parallel for collapse(2)
    for (i = 0; i < h; i++){
        for (j = 0; j < w; j++){
            int idx;
            idx = i * 4 * w + j * 4;
            grayScaleImg[i*w+j] = (unsigned char)(0.299*inputImage[idx]+0.587*inputImage[idx + 1]+0.114*inputImage[idx + 2]);
        }
    }
}
