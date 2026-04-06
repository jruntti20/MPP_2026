#include <stdlib.h>
#include <omp.h>

void occlusion_filling_nearest_pixel(unsigned char *readyOutputImg, unsigned char *outputImageLeftD, int img_h, int img_w, int range, int win_size);
void occlusion_filling_lininterp(unsigned char *readyOutputImg, unsigned char *outputImageLeftD, unsigned int img_h, unsigned int img_w, int win_size);
void fillSinglePixels(unsigned char *readyOutputImg, int img_w, int img_h, int window_size);
void fillSinglePixelsMedian(unsigned char *readyOutputImg, int img_w, int img_h, int window_size);
void normalize_disparity_map(unsigned char *readyOutputImg, int img_h, int img_w, int max_disp, int min_disp);
