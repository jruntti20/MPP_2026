#include <stdlib.h>
#include <iostream>
#include <stdlib.h>
#include <sys/time.h>
#include <vector>
#include "lodepng.h"
#include "zncc.h"
#include "imageManipulation.h"

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
    

    /* TODO: Commandline argument parsing, if I have time

    bool isCaseInsensitive = false;
    enum { NO_ARGS, WIN_SIZE, MAX_DISP, CC_THRESHOLD, OCCL_FILL_RANGE } mode = NO_ARGS;
    size_t optind;
    for (optind = 1; optind < argc && argv[optind][0] == '-'; optind++) {
        switch (argv[optind][1]) {
        case 'w':
            ; 
            break;
        case 'd': mode = MAX_DISP; break;
        case 't': mode = CC_THRESHOLD; break;
        case 'r': mode = OCCL_FILL_RANGE; break;
        default:
            fprintf(stderr, "Usage: %s [-wdtr] [file...]\n", argv[0]);
            exit(EXIT_FAILURE);
        }   
    }
    argv += optind;    
    */

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
    for (unsigned int u = 0; u < h/4; u++){
        for (unsigned int v = 0; v < w/4; v++){
            //gettimeofday(&pixeltime[0], NULL);
            if (((int) u - (win_size -1)/2 < 0) || ((int) u + (win_size -1)/2 >= (int)h/4) || ((int) v - (win_size -1)/2 < 0) || ((int) v + (win_size -1)/2 >= (int)w/4)){
                continue;
            }
            
            zncc(tinyGrayImageLeft,tinyGrayImageRight,outputImageLeftD, u, v, w/4, h/4, win_size, min_d_1, max_d_1, &loop_counter);
            zncc(tinyGrayImageRight,tinyGrayImageLeft,outputImageRightD, u, v, w/4, h/4, win_size, min_d_2, max_d_2, &loop_counter);
            //printf("%d\n", outputImageLeftD[u * w/4 + v]);
            //gettimeofday(&pixeltime[1], NULL);
            //printf("Image pixel counter: %ld", loop_counter);
            //printf(" Loop took time (microseconds): %ld\n", (pixeltime[1].tv_sec - pixeltime[0].tv_sec)*1000000L + (pixeltime[1].tv_usec - pixeltime[0].tv_usec));
            //printf("Loop counter: %ld\n", loop_counter);

        }
    }
    gettimeofday(&t[2], NULL);
    //int sum_disp_right = {};
    //int sum_disp_left = {};
    //for(size_t i = 0; i < WIDTH/4 * HEIGHT/4; i++){
    //    sum_disp_right += disparityValuesRight[i];
    //    sum_disp_left += disparityValuesLeft[i];
    //} 
    //std::cout << "Best overall disp value right (average): " << sum_disp_right / (w/4 * h/4) << std::endl;
    //std::cout << "Best overall disp value left (average): " << sum_disp_left / (w/4 * h/4) << std::endl;

    char filename_left_disp[128];
    char filename_right_disp[128];

    sprintf(filename_left_disp, "../Output_images/zncc_left_w%d_d%d.png", win_size, max_d_1);
    sprintf(filename_right_disp, "../Output_images/zncc_right_w%d_d%d.png", win_size, max_d_1);

    lodepng_encode_file(filename_left_disp, outputImageLeftD, w/4, h/4, LCT_GREY, 8);
    lodepng_encode_file(filename_right_disp, outputImageRightD, w/4, h/4, LCT_GREY, 8);
    // Cross checking
    gettimeofday(&t[3], NULL);
    int threshold_val = 15;
    for (unsigned int i = 0; i < h/4; i ++){
        for (unsigned int j = 0; j < w/4; j++){
            if (((int) i - (win_size -1)/2 < 0) || ((int) i + (win_size -1)/2 >= (int)h/4) || ((int) j - (win_size -1)/2 < 0) || ((int) j + (win_size -1)/2 >= (int)w/4)){
                continue;
            }
            if (abs(outputImageLeftD[j+i*(w/4)] - outputImageRightD[j+i*(w/4)]) > threshold_val){
                outputImageLeftD[j+i*(w/4)] = 0;
            }
        }
    }

    char filename_left_disp_cc[128];

    sprintf(filename_left_disp_cc, "../Output_images/zncc_left_cc_w%d_d%d_t%d.png", win_size, max_d_1, threshold_val);
    lodepng_encode_file(filename_left_disp_cc, outputImageLeftD, w/4, h/4, LCT_GREY, 8);

    // Occlusion filling
    gettimeofday(&t[4], NULL);
    int abort;
    unsigned int pixval;
    int range = 50;
    int wi = (int)w;
    int hi = (int)h;
    int left, right, top, bottom, x, y, r;
    //int left, right, x, r;
    for (int i = 0; i < hi/4; i++){
        for (int j = 0; j < wi/4; j++){
            if (((int) i - (win_size -1)/2 < 0) || ((int) i + (win_size -1)/2 >= (int)h/4) || ((int) j - (win_size -1)/2 < 0) || ((int) j + (win_size -1)/2 >= (int)w/4)){
                continue;
            }
            if (outputImageLeftD[j + i*(wi/4)] == 0){
                abort = 0;
                for (r = 0; r < range && !abort; r++){
                    left = j - r;
                    right = j + r;
                    top = i - r;
                    bottom = i + r;
                    if(left<0){left = 0;}
                    if(right>=wi/4){right = (wi/4)-1;}
                    if(top < 0){top = 0;}
                    if(bottom>=hi/4){bottom = (hi/4)-1;}
                        
                    pixval = 0;

                    for (x=left; x<=right && !abort; x++){
                        for (y = top; y <= bottom && !abort; y++){
                            if (outputImageLeftD[x + y*(wi/4)] > pixval){
                                pixval = outputImageLeftD[x + y*(wi/4)];
                            }
                        }
                    }
                    //printf("pixval = %d, ", pixval); 
                    //printf("pixval != 0: %d\n", pixval != 0);
                    if(pixval != 0){
                        readyOutputImg[j + i*(wi/4)] = pixval;
                        //printf("pixval updated to: %d\n", pixval);
                        abort = 1;
                    }
                }
            } else {
                readyOutputImg[j + i*(wi/4)] = outputImageLeftD[j + i*(wi/4)];
            }
        }
    }

    gettimeofday(&t[5], NULL);
    char filename[128];
    sprintf(filename, "../Output_images/zncc_postproc_ready_w%d_d%d_t%d_r%d.png", win_size, max_d_1, threshold_val, range);
    lodepng_encode_file(filename, readyOutputImg, w/4, h/4, LCT_GREY, 8);

    //open_cl_run(inputImage, kernelFileConv, kernelFileGray, w, h);
    
    //free(disparityValuesRight);
    //free(disparityValuesLeft);
    free(tinyGrayImageLeft);
    free(tinyGrayImageRight);
    free(grayInputImageLeft);
    free(grayInputImageRight);
    free(outputImageRightD);
    free(readyOutputImg);

    printf("Grayscale and resize (seconds): %ld\n", t[1].tv_sec - t[0].tv_sec);
    printf("Zncc (seconds): %ld\n", t[2].tv_sec - t[1].tv_sec);
    printf("Croschecking (seconds): %ld\n", t[4].tv_sec - t[3].tv_sec);
    printf("Occlusion filling (seconds): %ld\n", t[5].tv_sec - t[4].tv_sec);

    return 0;
}
