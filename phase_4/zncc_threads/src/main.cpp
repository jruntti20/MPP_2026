#include <stdlib.h>
#include <iostream>
#include <stdlib.h>
#include <sys/time.h>
#include <vector>
#include <thread>
#include <errno.h>
#include "lodepng.h"
//#include "zncc.h"
#include "imageManipulation.h"
#include "cross_checking.h"
#include "occlusion_filling.h"
#include "zncc_integral.h"

#include "pthread.h"

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

void inline arg_error(char *endptr, char *argv_a)
{
    if(endptr == argv_a  || *endptr != '\0' || errno == ERANGE)
    {
        printf("Please provide a valid integer as a parameter for %s.\n", argv_a);
        exit(1);
    }
}


void pad_image_1px(
    const unsigned char* src,
    int width,
    int height,
    unsigned char* dst)
{
    int padded_width = width + 2;

    // initialize entire padded image to zero
    //memset(dst, 0, (width + 2) * (height + 2) * sizeof(unsigned char));

    // copy original image into center
    for(int y = 0; y < height; y++)
    {
        for(int x = 0; x < width; x++)
        {
            dst[(y+1) * padded_width + (x+1)] = src[y * width + x];
        }
    }
}
int main(int argc, char ** argv){

    //default parameters
    int win_size = 23;
    int min_d_1 = 0;
    int max_d_1 = 65;
    int min_d_2 = max_d_1 * (-1);
    int max_d_2 = 0;
    int threshold_val = 8;
    int occlusion_window = 50;

    const char imgPathLeft[] = "../img/im0.png";
    const char imgPathRight[] = "../img/im1.png";
    //
    //define input image size parameters
    unsigned int w = WIDTH;
    unsigned int h = HEIGHT;

    char *endptr;

    // Command line arguments handling
    if (argc > 1)
    {
        for (int a = 1; a < argc; a++)
        {
            //strcpy(arc_buff, argv[a]);
            //if (strcmp(arc_buff, "-w") == 0)
            if (strcmp(argv[a], "-w") == 0)
            {
                win_size = strtol(argv[a + 1], &endptr, 10);
                arg_error(endptr, argv[a]);
                a++;
            }
            else if (strcmp(argv[a], "-d") == 0)
            {
                max_d_1 = strtol(argv[a + 1], &endptr, 10);
                min_d_2 = (-1) * max_d_1;
                arg_error(endptr, argv[a]);
                a++;
            }
            else if (strcmp(argv[a], "-c") == 0)
            {
                threshold_val = strtol(argv[a + 1], &endptr, 10); 
                arg_error(endptr, argv[a]);
                a++;
            }
            else if (strcmp(argv[a], "-ow") == 0)
            {
                occlusion_window = strtol(argv[a + 1], &endptr, 10);
                arg_error(endptr, argv[a]);
                a++;
            }
        }
    }
    

    /* TODO: Commandline argument parsing, if I have time*/

    // Source image path.

    // define output image and size.
    unsigned char *inputImageLeft = (unsigned char *)malloc(WIDTH * HEIGHT * 4 * sizeof(BYTE)); 
    unsigned char *inputImageRight = (unsigned char *)malloc(WIDTH * HEIGHT * 4 * sizeof(BYTE)); 
    unsigned char *grayInputImageLeft = (unsigned char *)malloc(WIDTH * HEIGHT * sizeof(BYTE)); 
    unsigned char *grayInputImageRight = (unsigned char *)malloc(WIDTH * HEIGHT * sizeof(BYTE)); 
    unsigned char *tinyGrayImageLeft = (unsigned char *)malloc(WIDTH/4 * HEIGHT/4 * sizeof(BYTE)); 
    unsigned char *tinyGrayImageRight = (unsigned char *)malloc(WIDTH/4 * HEIGHT/4 * sizeof(BYTE));

    // Add 1 pixel padding to resized images. Needed for integral image processing.
    unsigned char *tinyGrayImageLeftPadded = (unsigned char *)calloc((WIDTH/4 + 2) * (HEIGHT/4 + 2), sizeof(BYTE)); 
    unsigned char *tinyGrayImageRightPadded = (unsigned char *)calloc((WIDTH/4 + 2) * (HEIGHT/4 + 2), sizeof(BYTE));

    unsigned char *outputImageRightD = (unsigned char *)calloc((WIDTH/4 + 2) * (HEIGHT/4 + 2) , sizeof(BYTE));
    unsigned char *outputImageLeftD = (unsigned char *)calloc((WIDTH/4 + 2) * (HEIGHT/4 + 2) , sizeof(BYTE));
    unsigned char *readyOutputImg = (unsigned char *)calloc((WIDTH/4 + 2) * (HEIGHT/4 + 2) , sizeof(BYTE));
    
    // Integral image tables with one pixel padding
    uint32_t *inputIntegralL = (uint32_t *)calloc((WIDTH/4 + 2) * (HEIGHT/4 + 2), sizeof(uint32_t));
    uint32_t *inputIntegralR = (uint32_t *)calloc((WIDTH/4 + 2) * (HEIGHT/4 + 2), sizeof(uint32_t));
    uint64_t *inputIntegralSquaredL = (uint64_t *)calloc((WIDTH/4 + 2) * (HEIGHT/4 + 2), sizeof(uint64_t));
    uint64_t *inputIntegralSquaredR = (uint64_t *)calloc((WIDTH/4 + 2) * (HEIGHT/4 + 2), sizeof(uint64_t));

    float *meanTableL = (float *)calloc((WIDTH/4 + 2) * (HEIGHT/4 + 2) , sizeof(float));
    float *meanTableR = (float *)calloc((WIDTH/4 + 2) * (HEIGHT/4 + 2) , sizeof(float));
    float *varTable1 = (float *)calloc((WIDTH/4 + 2) * (HEIGHT/4 + 2) , sizeof(float));
    float *varTable2 = (float *)calloc((WIDTH/4 + 2) * (HEIGHT/4 + 2) , sizeof(float));
    
    if (!inputImageLeft || !inputImageRight || !tinyGrayImageLeft || !tinyGrayImageRight || !grayInputImageLeft || !grayInputImageRight || !outputImageLeftD || !outputImageRightD){
        std::cout << "Failed to malloc a buffer for an image" << std::endl;
    }
    
    // read image from imgPath into memory allocated for inputImage
    lodepng_decode_file(&inputImageLeft, &w, &h, imgPathLeft, LCT_RGBA, 8);
    lodepng_decode_file(&inputImageRight, &w, &h, imgPathRight, LCT_RGBA, 8);

    
    gettimeofday(&t[0], NULL);
    grayScaleImage(inputImageLeft, grayInputImageLeft, w, h);
    grayScaleImage(inputImageRight, grayInputImageRight, w, h);
    resizeImage(grayInputImageLeft, tinyGrayImageLeft, w, h);
    resizeImage(grayInputImageRight, tinyGrayImageRight, w, h);




    pad_image_1px(tinyGrayImageLeft, w/4, h/4, tinyGrayImageLeftPadded);
    pad_image_1px(tinyGrayImageRight, w/4, h/4, tinyGrayImageRightPadded);

    free(tinyGrayImageLeft);
    free(tinyGrayImageRight);

    lodepng_encode_file("../grayscale_image_left.png", grayInputImageLeft, w, h, LCT_GREY, 8);
    lodepng_encode_file("../tiny_image_left_padded.png", tinyGrayImageLeftPadded, w/4 + 2, h/4 + 2, LCT_GREY, 8);
 
    gettimeofday(&t[1], NULL);

    /*
    */
    unsigned char testImageL[16] = {5, 2, 5, 2, 3, 6, 3, 6, 5, 2, 5, 2, 3, 6, 3, 6};
    unsigned char testImageR[16] = {2, 5, 2, 6, 6, 3, 6, 3, 2, 5, 2, 6, 6, 3, 6, 3};
    unsigned char *testImagePaddedL = (unsigned char*)calloc((4+2) * (4+2), sizeof(unsigned char));
    unsigned char *testImagePaddedR = (unsigned char*)calloc((4+2) * (4+2), sizeof(unsigned char));

    pad_image_1px(testImageL, 4, 4, testImagePaddedL);
    pad_image_1px(testImageR, 4, 4, testImagePaddedR);


    uint32_t *testInputIntL = (uint32_t *)calloc((4+2) * (4+2), sizeof(uint32_t));
    uint32_t *testInputIntR = (uint32_t *)calloc((4+2) * (4+2), sizeof(uint32_t));
    uint64_t *testInputIntSquaredL = (uint64_t *)calloc((4+2) * (4+2), sizeof(uint64_t));
    uint64_t *testInputIntSquaredR = (uint64_t *)calloc((4+2) * (4+2), sizeof(uint64_t));
    float *testMeanTableL = (float *)calloc((4+2) * (4+2), sizeof(float));
    float *testMeanTableR = (float *)calloc((4+2) * (4+2), sizeof(float));
    float *testVarTableL = (float *)calloc((4+2) * (4+2), sizeof(float));
    float *testVarTableR = (float *)calloc((4+2) * (4+2), sizeof(float));

    int padded_width = 6; 
    int padded_height = 6;

    populate_integral_tables(testImagePaddedL , testImagePaddedR , testInputIntL , testInputIntR , testInputIntSquaredL , testInputIntSquaredR , testMeanTableL , testMeanTableR , testVarTableL , testVarTableR , padded_height , padded_width , 3);
    printf("Original testImageL:\n");
    for(int y = 0; y < 4 ; y++)
    {
        for(int x = 0; x < 4 ; x++)
        {
            printf(" %u",testImageL[y * 4 + x]);
        }
        printf("\n");
    }
    printf("Padded testImageL:\n");
    for(int y = 0; y < padded_height ; y++)
    {
        for(int x = 0; x < padded_width ; x++)
        {
            printf(" %u",testImagePaddedL[y * padded_width + x]);
        }
        printf("\n");
    }
    printf("Integral image testImageL:\n");
    for(int y = 0; y < padded_height ; y++)
    {
        for(int x = 0; x < padded_width ; x++)
        {
            printf("%lu ",(unsigned long)testInputIntL[y * padded_width + x]);
        }
        printf("\n");
    }
    printf("Squared integral image testImageL:\n");
    for(int y = 0; y < padded_height ; y++)
    {
        for(int x = 0; x < padded_width ; x++)
        {
            printf("%llu ",(unsigned long long)testInputIntSquaredL[y * padded_width + x]);
        }
        printf("\n");
    }
    printf("MeanTableL:\n");
    for(int y = 0; y < padded_height ; y++)
    {
        for(int x = 0; x < padded_width ; x++)
        {
            printf("%f ",testMeanTableL[y * padded_width + x]);
        }
        printf("\n");
    }
    printf("VarTableL:\n");
    for(int y = 0; y < padded_height ; y++)
    {
        for(int x = 0; x < padded_width ; x++)
        {
            printf("%f ",testVarTableL[y * padded_width + x]);
        }
        printf("\n");
    }
    //populate_integral_tables(tinyGrayImageLeftPadded, tinyGrayImageRightPadded, inputIntegralL, inputIntegralR, inputIntegralSquaredL, inputIntegralSquaredR, meanTableL, meanTableR, varTable1, varTable2, h/4 + 2, w/4 + 2, win_size);

    //int num_threads = std::thread::hardware_concurrency();
    int num_threads = 1;
    ThreadData tdata[num_threads];
    //int rows_per_thread = ((h / 4) - (win_size - 1)) / num_threads;
    int rows_per_thread = (h / 4 + 2)  / num_threads;
    
    std::vector<std::thread> threads1;
    for(int t = 0; t < num_threads; t++)
    {
        tdata[t].img_h = h/4 + 2;
        tdata[t].img_w = w/4 + 2;
        tdata[t].window_size = win_size;
        tdata[t].max_disp = max_d_1;
        tdata[t].disp_sign = 1;
        tdata[t].left = inputIntegralL;
        tdata[t].right = inputIntegralR;
        tdata[t].leftSquared = inputIntegralSquaredL;
        tdata[t].rightSquared = inputIntegralSquaredR;
        tdata[t].meanL = meanTableL;
        tdata[t].meanR = meanTableR;
        tdata[t].varL = varTable1;
        tdata[t].varR = varTable2;
        tdata[t].disparity = outputImageLeftD;

        //tdata[t].y_start = t * rows_per_thread + win_size - 1;
        //tdata[t].y_end = (t==num_threads - 1) ? tdata[t].img_h - win_size - 1 : (t+1)*rows_per_thread;
        tdata[t].y_start = t * rows_per_thread;
        tdata[t].y_end = (t==num_threads - 1) ? tdata[t].img_h  : (t+1)*rows_per_thread;

        threads1.emplace_back(zncc_worker, &tdata[t]);
    }
    for(auto& th: threads1)
    {
        th.join();
    }

    std::vector<std::thread> threads2;
    for(int t = 0; t < num_threads; t++)
    {
        tdata[t].img_h = h/4 + 2;
        tdata[t].img_w = w/4 + 2;
        tdata[t].window_size = win_size;
        tdata[t].max_disp = max_d_1;
        tdata[t].disp_sign = -1;
        tdata[t].left = inputIntegralR;
        tdata[t].right = inputIntegralL;
        tdata[t].leftSquared = inputIntegralSquaredR;
        tdata[t].rightSquared = inputIntegralSquaredL;
        tdata[t].meanL = meanTableR;
        tdata[t].meanR = meanTableL;
        tdata[t].varL = varTable2;
        tdata[t].varR = varTable1;
        tdata[t].disparity = outputImageRightD;


        //tdata[t].y_start = t * rows_per_thread + win_size - 1;
        //tdata[t].y_end = (t==num_threads - 1) ? tdata[t].img_h - win_size - 1 : (t+1)*rows_per_thread;
        tdata[t].y_start = t * rows_per_thread;
        tdata[t].y_end = (t==num_threads - 1) ? tdata[t].img_h : (t+1)*rows_per_thread;

        threads2.emplace_back(zncc_worker, &tdata[t]);
    }
    for(auto& th: threads2)
    {
        th.join();
    }



    //zncc_integral(tinyGrayImageLeft,tinyGrayImageRight,outputImageLeftD, w/4, h/4, win_size, min_d_1, max_d_1, &loop_counter);    


    /*
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
    */
    gettimeofday(&t[2], NULL);
    char filename_left_disp[128];
    char filename_right_disp[128];

    sprintf(filename_left_disp, "../Output_images/zncc_left_w%d_d%d.png", win_size, max_d_1);
    sprintf(filename_right_disp, "../Output_images/zncc_right_w%d_d%d.png", win_size, max_d_1);

    lodepng_encode_file(filename_left_disp, outputImageLeftD, w/4 + 2, h/4 + 2, LCT_GREY, 8);
    lodepng_encode_file(filename_right_disp, outputImageRightD, w/4 + 2, h/4 + 2, LCT_GREY, 8);

    // Cross checking
    gettimeofday(&t[3], NULL);

    cross_checking(outputImageLeftD, outputImageRightD, h, w, threshold_val, win_size);

    char filename_left_disp_cc[128];

    sprintf(filename_left_disp_cc, "../Output_images/zncc_left_cc_w%d_d%d_t%d.png", win_size, max_d_1, threshold_val);
    lodepng_encode_file(filename_left_disp_cc, outputImageLeftD, w/4 + 2, h/4 + 2, LCT_GREY, 8);

    gettimeofday(&t[4], NULL);
    // Occlusion filling
    //occlusion_filling_lininterp(readyOutputImg, outputImageLeftD, h/4, w/4, win_size);
    occlusion_filling_nearest_pixel(readyOutputImg, outputImageLeftD, h/4 + 2, w/4 + 2, occlusion_window, win_size);
    normalize_disparity_map(readyOutputImg, h/4 + 2, w/4 + 2, min_d_1, max_d_1);

    gettimeofday(&t[5], NULL);
    char filename[128];
    sprintf(filename, "../Output_images/zncc_postproc_ready_w%d_d%d_t%d_lininterp.png", win_size, max_d_1, threshold_val);
    lodepng_encode_file(filename, readyOutputImg, w/4 + 2, h/4 + 2, LCT_GREY, 8);

    free(tinyGrayImageLeftPadded);
    free(tinyGrayImageRightPadded);
    free(grayInputImageLeft);
    free(grayInputImageRight);
    free(outputImageRightD);
    free(readyOutputImg);
    free(inputIntegralL);
    free(inputIntegralR);
    free(inputIntegralSquaredL);
    free(inputIntegralSquaredR);
    free(meanTableL);
    free(meanTableR);
    free(varTable1);
    free(varTable2);


    /*
    */
    free(testInputIntL);
    free(testInputIntR);
    free(testInputIntSquaredL);
    free(testInputIntSquaredR);
    free(testMeanTableL);
    free(testMeanTableR);
    free(testVarTableL);
    free(testVarTableR);

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
