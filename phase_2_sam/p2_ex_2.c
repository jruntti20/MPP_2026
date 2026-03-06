#include <stdio.h>
#include <stdlib.h>
#include "lodepng.h"

#define WIDTH 2940
#define HEIGHT 2016
#define CHANNELS 4

typedef unsigned char BYTE;

int main()
{
    const char imgPath[] = "im0.png";

    unsigned int w = WIDTH;
    unsigned int h = HEIGHT;

    BYTE* inputImage = (BYTE*) malloc(WIDTH * HEIGHT * CHANNELS);
    BYTE* resized = (BYTE*) malloc(WIDTH/4 * HEIGHT/4 * CHANNELS);
    BYTE* gray = (BYTE*) malloc(WIDTH/4 * HEIGHT/4);
    BYTE* filtered = (BYTE*) malloc(WIDTH/4 * HEIGHT/4);

    if(!inputImage || !resized || !gray || !filtered)
    {
        printf("Memory allocation failed\n");
        return 1;
    }

    // Read image
    lodepng_decode_file(&inputImage, &w, &h, imgPath, LCT_RGBA, 8);

    printf("Image size: %u x %u\n", w, h);

    // Resize
    for(int y=0;y<h/4;y++)
    {
        for(int x=0;x<w/4;x++)
        {
            int srcX = x*4;
            int srcY = y*4;

            for(int c=0;c<4;c++)
            {
                resized[(y*(w/4)+x)*4 + c] =
                inputImage[(srcY*w + srcX)*4 + c];
            }
        }
    }

    // Grayscale
    for(int i=0;i<(w/4)*(h/4);i++)
    {
        BYTE r = resized[i*4 + 0];
        BYTE g = resized[i*4 + 1];
        BYTE b = resized[i*4 + 2];

        gray[i] = 0.2126*r + 0.7152*g + 0.0722*b;
    }

    // 5x5 filter
    for(int y=2;y<h/4-2;y++)
    {
        for(int x=2;x<w/4-2;x++)
        {
            int sum = 0;

            for(int ky=-2; ky<=2; ky++)
            {
                for(int kx=-2; kx<=2; kx++)
                {
                    sum += gray[(y+ky)*(w/4) + (x+kx)];
                }
            }

            filtered[y*(w/4)+x] = sum/25;
        }
    }

    // Save grayscale
    lodepng_encode_file("gray.png", gray, w/4, h/4, LCT_GREY, 8);

    // Save filtered
    lodepng_encode_file("filtered.png", filtered, w/4, h/4, LCT_GREY, 8);

    free(inputImage);
    free(resized);
    free(gray);
    free(filtered);

    return 0;
}