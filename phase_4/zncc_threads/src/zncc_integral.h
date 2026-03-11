#ifndef _ZNCC_H
#define _ZNCC_H

#include <stdint.h>
#include <tuple>
#include <inttypes.h>
#include <math.h>
#include <sys/time.h>

struct ThreadData
{
    int y_start;
    int y_end;

    int img_w;
    int img_h;
    int window_size;
    int max_disp;
    int disp_sign;

    uint32_t *left;
    uint32_t *right;
    uint32_t *leftSquared;
    uint32_t *rightSquared;

    float *meanL;
    float *meanR;

    float *varL;
    float *varR;

    unsigned char *disparity;

};

void zncc_worker(ThreadData *data);

void populate_integral_tables(unsigned char *inputImage1, unsigned char *inputImage2, uint32_t *inputIntegralL, uint32_t *inputIntegralR, uint32_t *inputIntegralSquaredL, uint32_t *inputIntegralSquaredR, float *meanTableL, float *meanTableR, float *varTableL, float *varTableR, int img_h, int img_w, int win_size);


#endif

