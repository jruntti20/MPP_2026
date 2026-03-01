#ifndef _ZNCC_H
#define _ZNCC_H

#include <stdint.h>
#include <tuple>

void zncc(unsigned char *inputImageL, unsigned char* inputImageR, unsigned char * outputImage, unsigned int u, unsigned int v, unsigned int img_w, unsigned int img_h, unsigned int win_size, int min_d, int max_d, unsigned long *loop_counter);

#endif
