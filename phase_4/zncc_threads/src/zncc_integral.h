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

    uint32_t *meanL;
    uint32_t *meanR;

    uint32_t *varL;
    uint32_t *varR;

    unsigned char *disparity;

};

void zncc_worker(ThreadData *data);

void populate_integral_tables(unsigned char *inputImage1, unsigned char *inputImage2, uint32_t *input1Integral, uint32_t *input2Integral, uint32_t *input1IntegralSquared, uint32_t *input2IntegralSquared, uint32_t *meanTable1, uint32_t *meanTable2, uint32_t *varTable1, uint32_t *varTable2, int img_h, int img_w, int win_size);


#endif

