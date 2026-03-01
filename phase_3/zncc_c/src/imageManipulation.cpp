#include <errno.h>
#include <stdio.h>
#include "lodepng.h"

#define WIDTH 2940
#define HEIGHT 2016
/*
//TODO Solve the segmentation fault

void ReadImage(char* imgPath, unsigned char* inputImage){

    unsigned int w = WIDTH;
    unsigned int h = HEIGHT;
    uint32_t Error = 0;

    Error = lodepng_decode_file(&inputImage, &w, &h, imgPath, LCT_RGBA, 8);
    if (Error){
        printf("Error in reading the left or right input image %u: %s\n", Error, lodepng_error_text(Error));
    }
    if(errno) printf("Value of errno: %d\n", errno);
}
*/
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

    int i,j;
    for (i = 0; i < h; i++){
        for (j = 0; j < w; j++){
            int idx;
            idx = i * 4 * w + j * 4;
            grayScaleImg[i*w+j] = (unsigned char)(0.299*inputImage[idx]+0.587*inputImage[idx + 1]+0.114*inputImage[idx + 2]);
        }
    }

    //    grayScaleImg[i] = (inputImage[i*4]*0.299 
    //            + inputImage[1 + i * 4]*0.587 
    //            + inputImage[2 + i * 4]*0.114);
    //}
}
