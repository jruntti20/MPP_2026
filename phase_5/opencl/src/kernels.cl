#define MAX_DISPARITY 64
#define BLOCK_SIZE 16
typedef unsigned char BYTE;


#define DEBUG_BUFFER_SIZE 8192

inline int dbg_alloc(__global int* index, int length)
{
    return atomic_add(index, length);
}

inline void dbg_write_str(__global char *buf, int offset, __constant char *str)
{
    int i = 0;
    while (str[i] != '\0')
    {
        buf[offset + i] = str[i];
        i++;
    }
}

inline int dbg_write_int(__global char *buf, int offset, int value)
{
    int start = offset;

    if (value == 0)
    {
        buf[offset++] = '0';
        return 1;
    }
    if (value < 0)
    {
        buf[offset++] = '-';
        value = -value;
    }
    int digits[10];
    int d = 0;

    while(value > 0)
    {
        digits[d++] = value % 10;
        value /= 10;
    }

    for (int i = d - 1; i >= 0; i--)
    {
    buf[offset++] = '0' + digits[i];
    }
    return offset - start;
}

inline int dbg_write_float(__global char *buf, int offset, float value)
{
    int start = offset;

    int int_part = (int)value;
    float frac = fabs(value - (float)int_part);

    offset += dbg_write_int(buf, offset, int_part);

    buf[offset++] = '.';

    // 3 decimals
    for(int i = 0; i < 3; i++)
    {
        frac *= 10.0f;
        int digit = (int)frac;
        buf[offset++] = '0' + digit;
        frac -= digit;
    }

    return offset - start;
}

inline void dbg_printf(__global char *buf, __global int* index, __constant char *fmt, float a, float b, float c)
{
    int offset = dbg_alloc(index, 128);
    int i = 0;
    int arg_i = 0;

    while (fmt[i] != '\0')
    {
        if (fmt[i] == '%')
        {
            i++;

            if (fmt[i] == 'f')
            {
                switch(arg_i)
                {
                    case 0:
                        offset += dbg_write_float(buf, offset, a);
                        break;
                    case 1:
                        offset += dbg_write_float(buf, offset, b);
                        break;
                    case 2:
                        offset += dbg_write_float(buf, offset, c);
                        break;
                }
                arg_i++;
            }
            //else if (fmt[i] == 'd')
            //{
            //    offset += dbg_write_int(buf, offset, c);
            //}
        }
        else
        {
            buf[offset++] = fmt[i];
        }
        i++;
    }
    buf[offset++] = '\n';
    buf[offset] = '\0';
}

__kernel void testKernel(__global char *debug_buf, __global int *debug_index)
{
    int gid_y = get_global_id(0);
    int gid_x = get_global_id(1);

    int size_y = get_global_size(0);
    int size_x = get_global_size(1);

    float val = gid_x * 1.234f;
    //dbg_printf(debug_buf, debug_index, "g_size_x=%d val=%f gid_x=%d", size_x, val, gid_x);
    //dbg_printf(debug_buf, debug_index, "gid_x: %f, gid_y %f, size_x: %f\n", gid_x, gid_y, size_x);
}

__kernel void zncc_fast(
    __global uchar* left,
    __global uchar* right,
    __global uchar* disparity,
    int disp_sign,
    int width,
    int height,
    int window,
    __global char *debug_buf,
    __global int *debug_index)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    
    int group_x = get_group_id(0);
    int group_y = get_group_id(1);

    int group_x_size = get_num_groups(0);
    int group_y_size = get_num_groups(1);

    int size_x = get_global_size(0);
    int size_y = get_global_size(1);


    //disparity[y*width+x] = 128;
    //return;

    //if (x == 0 && y == 0)
    //{
    //    dbg_printf(debug_buf, debug_index, "x: %f, y %f, size_x: %f\n", x, y, size_x);
    //}

    if(x>=width || y >= height)
        return;

    if(x < window || y < window || x >= width-window || y >= height-window)
    {
        disparity[y*width+x] = 0;
        return;
    }

    //if (!(x + (d * disp_sign) < width - window))
    //    return;

    int best_d = 0;
    float best_score = -2.0f;

    for(int d=0; d<MAX_DISPARITY && x+d < width-window; d++)
    {

        float sumL=0,sumR=0,sumLR=0,sumL2=0,sumR2=0;
        int count=0;

        for(int wy=-window; wy<=window; wy++)
        for(int wx=-window; wx<=window; wx++)
        {
            int xl=x+wx;
            int yl=y+wy;
            int xr=xl-(d*disp_sign);

            int idxL=yl*width+xl;
            int idxR=yl*width+xr;

            //int xr = x + wx - d * disp_sign;
            //if(xr < 0 || xr >= width)
            //    continue;

            //if (x == 100 && y == 100)
            //{
            //    dbg_printf(debug_buf, debug_index, "d: %f, idxL: %f, idxR: %f\n", d, idxL, idxR);
            //}
            
            float L = left[idxL];
            //float L = (float)tileL[ty * tile_w + tx];

            float R = right[idxR];
            //float R = (float)tileR[ty * tile_w + tx_r];

            //if (x == 100 && y == 100)
            //{
            //    dbg_printf(debug_buf, debug_index, "d: %f, L: %f, R: %f\n", d, L, R);
            //}

            sumL+=L;
            sumR+=R;
            sumLR+=L*R;
            sumL2+=L*L;
            sumR2+=R*R;

            count++;
        }

        if(count == 0)
            continue;

        float numerator = count*sumLR - sumL*sumR;
        float denomL = count*sumL2 - sumL*sumL;
        float denomR = count*sumR2 - sumR*sumR;

        float denom = denomL * denomR;

        if(denom < 1e-5f)
            continue;

        float score = numerator / sqrt(denomL * denomR);

        if(score > best_score)
        {
            best_score = score;
            best_d = d;
        }
        
        //if (x == 100 && y == 100)
        //{
        //    dbg_printf(debug_buf, debug_index, "d: %f, numerator: %f, score: %f\n", d, numerator, score);
        //}
    }

    //disparity[y*width+x] = (uchar)(best_d * 255 / MAX_DISPARITY);
    disparity[y*width+x] = best_d;
}

__kernel void resize_grayscale_image(__global const BYTE *inputBuf1, __global const BYTE *inputBuf2, __global BYTE *outputBuf1, __global BYTE *outputBuf2, uchar resize_factor, uint w)
{
    int i = get_global_id(0);
    int j = get_global_id(1);
    uint new_w = w/resize_factor;
    ulong idx = ((unsigned int)(i * w + j)) * 4 * resize_factor; // Take resize_factor:th pixels (4 bytes per pixel) from original image
    outputBuf1[i*new_w + j] = (BYTE)(inputBuf1[idx]*0.299 + inputBuf1[idx + 1]*0.587 + inputBuf1[idx + 2] * 0.114);
    outputBuf2[i*new_w + j] = (BYTE)(inputBuf2[idx]*0.299 + inputBuf2[idx + 1]*0.587 + inputBuf2[idx + 2] * 0.114);
}

__kernel void cross_checking(__global BYTE *inputImageL, __global BYTE *inputImageR, __global BYTE *outputImgL, int width, int height, int threshold, int win_size, __global char *debug_buf, __global int *debug_index)
{
    int i = get_global_id(1);
    int j = get_global_id(0);

    //if (i == 0 && j == 0)
    //{
    //    dbg_printf(debug_buf, debug_index, "Cross checking: i: %f, j %f, 1: %f\n", i, j, 1);
    //}

    int idx = j+i*width;

    int dL = inputImageL[idx];

    int jr = j - dL; // corresponding pixel in right image
    
    if(jr < 0 || jr >= width)
    {
        outputImgL[idx] = 0;
        outputImgL[idx] = inputImageL[idx];
        return;
    }

    int dR = inputImageR[jr+i*width];

    if (abs(dL - dR) > threshold)
    {
        outputImgL[idx] = 0;
    }
    else
    {
        outputImgL[idx] = dL;
    }
}

__kernel void occlusion_filling(__global BYTE *readyOutputImg, __global BYTE *inputImageL, int width, int height, int range, int win_size)
{
    int i = get_global_id(1);
    int j = get_global_id(0);

    float pixval;
    int left, right, top, bottom, x, y, rp, rn;

    if ((i - (win_size -1)/2 < 0) || ( i + (win_size -1)/2 >= height)|| ( j - (win_size -1)/2 < 0) || (j + (win_size -1)/2 >= width))
    {
        return;
    }

    int idx = j+i*width;
    if(inputImageL[idx] == 0.0f)
    {
        for (rp = 0, rn = 0; rp < range && rn > -range; rp++, rn--)
        {
            left = j + rn;
            right = j + rp;
            top = i + rn;
            bottom = i + rp;
            if(left<0){left = 0;}
            if(right>=width){right = width - 1;}
            if(top < 0){top = 0;}
            if(bottom>=height){bottom = height - 1;}
                
            pixval = 0.0f;

            for (x=left; x<=right; x++)
            {
                for (y = top; y <= bottom; y++)
                {
                    if (inputImageL[x + y*width] > pixval)
                    {
                        pixval = inputImageL[x + y*width];
                    }
                }
            }
            if(pixval != 0.0f){
                readyOutputImg[idx] = pixval;
                break;
            }
        }
    } else {
        readyOutputImg[idx] = inputImageL[idx];
    }
}

