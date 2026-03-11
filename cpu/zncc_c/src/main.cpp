#include <stdlib.h>
#include <iostream>
#include <stdlib.h>
#include <sys/time.h>
#include <vector>
#include "lodepng.h"
#include "zncc.h"
#include "imageManipulation.h"
#include "cross_checking.h"
#include "occlusion_filling.h"

#define BUF_SIZE 0x100000
#define IMG_BUF 0x1000000
#define WIDTH 2940
#define HEIGHT 2016
#define CHANNELS 4

typedef unsigned char BYTE;

// define the gaussianBlur filter
struct timeval t[6];
struct timeval pixeltime[2];

unsigned long loop_counter = 0;

int main(int argc, char ** argv){
    

    /* TODO: Commandline argument parsing, if I have time*/

    // Source image path.
    const char imgPathLeft[] = "../img/im0.png";
    const char imgPathRight[] = "../img/im1.png";
    //
    //define input image size parameters
    unsigned int w = WIDTH;
    unsigned int h = HEIGHT;

    // define output image and size.
    unsigned char *inputImageLeft = (unsigned char *)malloc(WIDTH * HEIGHT * 4 * sizeof(BYTE)); 
    unsigned char *inputImageRight = (unsigned char *)malloc(WIDTH * HEIGHT * 4 * sizeof(BYTE)); 
    unsigned char *tinyGrayImageLeft = (unsigned char *)malloc(WIDTH/4 * HEIGHT/4 * sizeof(BYTE)); 
    unsigned char *tinyGrayImageRight = (unsigned char *)malloc(WIDTH/4 * HEIGHT/4 * sizeof(BYTE)); 
    unsigned char *grayInputImageLeft = (unsigned char *)malloc(WIDTH * HEIGHT * sizeof(BYTE)); 
    unsigned char *grayInputImageRight = (unsigned char *)malloc(WIDTH * HEIGHT * sizeof(BYTE)); 

    unsigned char *outputImageRightD = (unsigned char *)calloc(WIDTH/4 * HEIGHT/4 , sizeof(BYTE));
    unsigned char *outputImageLeftD = (unsigned char *)calloc(WIDTH/4 * HEIGHT/4 , sizeof(BYTE));
    unsigned char *readyOutputImg = (unsigned char *)calloc(WIDTH/4 * HEIGHT/4 , sizeof(BYTE));
    
    if (!inputImageLeft || !inputImageRight || !tinyGrayImageLeft || !tinyGrayImageRight || !grayInputImageLeft || !grayInputImageRight || !outputImageLeftD || !outputImageRightD){
        std::cout << "Failed to malloc a buffer for an image" << std::endl;
    }
    
    // read image from imgPath into memory allocated for inputImage
    lodepng_decode_file(&inputImageLeft, &w, &h, imgPathLeft, LCT_RGBA, 8);
    lodepng_decode_file(&inputImageRight, &w, &h, imgPathRight, LCT_RGBA, 8);

    int win_size = 23;
    int min_d_1 = 0;
    int max_d_1 = 65;
    int min_d_2 = max_d_1 * (-1);
    int max_d_2 = 0;
    
    gettimeofday(&t[0], NULL);
    grayScaleImage(inputImageLeft, grayInputImageLeft, w, h);
    grayScaleImage(inputImageRight, grayInputImageRight, w, h);
    resizeImage(grayInputImageLeft, tinyGrayImageLeft, w, h);
    resizeImage(grayInputImageRight, tinyGrayImageRight, w, h);

    lodepng_encode_file("../grayscale_image_left.png", grayInputImageLeft, w, h, LCT_GREY, 8);
    lodepng_encode_file("../tiny_image_left.png", tinyGrayImageLeft, w/4, h/4, LCT_GREY, 8);
 
    gettimeofday(&t[1], NULL);
    for (unsigned int u = 0; u < h/4; u++)
    {
        for (unsigned int v = 0; v < w/4; v++)
        {
            if (((int) u - (win_size -1)/2 < 0) || ((int) u + (win_size -1)/2 >= (int)h/4) || ((int) v - (win_size -1)/2 < 0) || ((int) v + (win_size -1)/2 >= (int)w/4))
            {
                continue;
            }
            
            zncc(tinyGrayImageLeft,tinyGrayImageRight,outputImageLeftD, u, v, w/4, h/4, win_size, min_d_1, max_d_1, &loop_counter);
            zncc(tinyGrayImageRight,tinyGrayImageLeft,outputImageRightD, u, v, w/4, h/4, win_size, min_d_2, max_d_2, &loop_counter);
        }
    }
    gettimeofday(&t[2], NULL);
    char filename_left_disp[128];
    char filename_right_disp[128];

    sprintf(filename_left_disp, "../Output_images/zncc_left_w%d_d%d.png", win_size, max_d_1);
    sprintf(filename_right_disp, "../Output_images/zncc_right_w%d_d%d.png", win_size, max_d_1);

    lodepng_encode_file(filename_left_disp, outputImageLeftD, w/4, h/4, LCT_GREY, 8);
    lodepng_encode_file(filename_right_disp, outputImageRightD, w/4, h/4, LCT_GREY, 8);

    // Cross checking
    gettimeofday(&t[3], NULL);

    int threshold_val = 8;
    cross_checking(outputImageLeftD, outputImageRightD, h, w, threshold_val, win_size);

    char filename_left_disp_cc[128];

    sprintf(filename_left_disp_cc, "../Output_images/zncc_left_cc_w%d_d%d_t%d.png", win_size, max_d_1, threshold_val);
    lodepng_encode_file(filename_left_disp_cc, outputImageLeftD, w/4, h/4, LCT_GREY, 8);

    gettimeofday(&t[4], NULL);
    // Occlusion filling
    //occlusion_filling_lininterp(readyOutputImg, outputImageLeftD, h/4, w/4, win_size);
    occlusion_filling_nearest_pixel(readyOutputImg, outputImageLeftD, h/4, w/4, 50, win_size);
    normalize_disparity_map(readyOutputImg, h/4, w/4, min_d_1, max_d_1);

    gettimeofday(&t[5], NULL);
    char filename[128];
    sprintf(filename, "../Output_images/zncc_postproc_ready_w%d_d%d_t%d_lininterp.png", win_size, max_d_1, threshold_val);
    lodepng_encode_file(filename, readyOutputImg, w/4, h/4, LCT_GREY, 8);

    free(tinyGrayImageLeft);
    free(tinyGrayImageRight);
    free(grayInputImageLeft);
    free(grayInputImageRight);
    free(outputImageRightD);
    free(readyOutputImg);

    // Profiling
    long seconds_1 = t[1].tv_sec - t[0].tv_sec;
    long useconds_1 = t[1].tv_usec - t[0].tv_usec;
    if(useconds_1 < 0){seconds_1--; useconds_1 += 1e6;}
    long milliseconds_1 = seconds_1 * 1000.0 + useconds_1 / 1000.0;

    long seconds_2 = t[2].tv_sec - t[1].tv_sec;
    long useconds_2 = t[2].tv_usec - t[1].tv_usec;
    if(useconds_2 < 0){seconds_2--; useconds_2 += 1e6;}
    long milliseconds_2 = seconds_2 * 1000.0 + useconds_2 / 1000.0;

    long seconds_3 = t[3].tv_sec - t[2].tv_sec;
    long useconds_3 = t[3].tv_usec - t[2].tv_usec;
    if(useconds_3 < 0){seconds_3--; useconds_3 += 1e6;}
    long milliseconds_3 = seconds_3 * 1000.0 + useconds_3 / 1000.0;

    long seconds_4 = t[4].tv_sec - t[3].tv_sec;
    long useconds_4 = t[4].tv_usec - t[3].tv_usec;
    if(useconds_4 < 0){seconds_4--; useconds_4 += 1e6;}
    long milliseconds_4 = seconds_4 * 1000.0 + useconds_4 / 1000.0;

    printf("Grayscale and resize (milliseconds): %ld\n", milliseconds_1);
    printf("Zncc (milliseconds): %ld\n", milliseconds_2);
    printf("Croschecking (milliseconds): %ld\n", milliseconds_3);
    printf("Occlusion filling (milliseconds): %ld\n", milliseconds_4);

    return 0;
}
