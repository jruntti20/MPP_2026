#pragma OPENCL EXTENSION cl_khr_int64_base_atomics : enable
#pragma OPENCL EXTENSION cl_khr_int64_extended_atomics : enable
#pragma OPENCL EXTENSION cl_khr_fp64 : enable

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

inline float box_sum(__global ulong* I, int x0, int y0, int x1, int y1, int w)
    {
    return I[y1 * w + x1] - I[y0 * w + x1] - I[y1 * w + x0] + I[y0 * w + x0];
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

    if (x == 0 && y == 0)
    {
        dbg_printf(debug_buf, debug_index, "size_x: %f, size_y %f, x: %f\n", size_x, size_y, x);
    }

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


__kernel void zncc_fast_integral(
        __global uchar *left,
        __global uchar *right,
        __global float *meanTableL,
        __global float *meanTableR,
        __global float *varTableL,
        __global float *varTableR,
        __global uchar *disparity,
        int disp_sign,
        int width,
        int height,
        int window,
        __local uchar* tileL
        )
{
    int x = get_global_id(0);
    int y = get_global_id(1);

    int lx = get_local_id(0);
    int ly = get_local_id(1);

    int group_x = get_group_id(0);
    int group_y = get_group_id(1);

    int local_w = get_local_size(0);
    int local_h = get_local_size(1);

    int r = window / 2;
    int N = window * window;

    int tile_w = local_w + 2 * r;
    int tile_h = local_h + 2 * r;

    int start_x = group_x * local_w - r;
    int start_y = group_y * local_h - r;

    barrier(CLK_LOCAL_MEM_FENCE);

    for (int ty = ly; ty < tile_h; ty += local_h)
        {
        for (int tx = lx; tx < tile_w; tx += local_w)
            {
            int img_x = start_x + tx;
            int img_y = start_y + ty;

            int tile_idx = ty * tile_w + tx;

            if (img_x >= 0 && img_x < width &&
                img_y >= 0 && img_y < height)
                {
                tileL[tile_idx] = left[img_y * width + img_x];
                }
            else
                {
                tileL[tile_idx] = 0;
                }
            }
        }

    barrier(CLK_LOCAL_MEM_FENCE);

    if(x>=width || y >= height)
        return;

    if(x < r || y < r || x >= width-r || y >= height-r)
    {
        disparity[y*width+x] = 0;
        return;
    }

    //if (!(x + (d * disp_sign) < width - window))
    //    return;

    int local_x = lx + r;
    int local_y = ly + r;

    int best_d = 0;
    float best_score = -2.0f;

    float meanL = meanTableL[y * width + x];
    float varL = varTableL[y * width + x];
    if(varL < 1e-6f) return;

    for(int d=0; d<MAX_DISPARITY && x+d < width-window; d++)
    {
        int xr = x - d * disp_sign;
        if (xr - r < 0 || xr + r >= width) continue;

        float meanR = meanTableR[y * width + xr];
        float varR = varTableR[y * width + xr];
        if(varR < 1e-6f) continue;

        float dot = 0.0f;

        for(int j = -r; j <= r; j++)
        for(int i = -r; i <= r; i++)
        {
            int tx = local_x + j;
            int ty = local_y + i;

            float L = (float)tileL[ty * tile_w + tx];
            float R = (float)right[(y + j) * width + xr + i];

            //float a = left[(y + j) * width + x + i];
            //float b = right[(y + j) * width + xr + i];

            //dot += a*b;
            dot += L*R;
        }

        float numerator = dot - N * meanL * meanR;
        float denom = sqrt(varL * varR) * N + 1e-6f;

        float zncc = numerator / denom;

        if (zncc > best_score)
        {
            best_score = zncc;
            best_d = d;
        }
    }

    disparity[y * width + x] = best_d;

}

__kernel void resize_grayscale_image(__global const BYTE *inputBuf1, __global const BYTE *inputBuf2, __global BYTE *outputBuf1, __global BYTE *outputBuf2, uchar resize_factor, uint w)
{
    int i = get_global_id(1);
    int j = get_global_id(0);
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
                //readyOutputImg[idx] = pixval;
                readyOutputImg[idx] = (uchar)(pixval * 255 / MAX_DISPARITY);
                break;
            }
        }
    } else {
        //readyOutputImg[idx] = inputImageL[idx];
        readyOutputImg[idx] = (uchar)(inputImageL[idx] * 255 / MAX_DISPARITY);
    }
}

__kernel void pad_image_1px(
    __global const uchar* inputImageL,
    __global const uchar* inputImageR,
    int w,
    int h,
    __global uchar* outputImageL,
    __global uchar* outputImageR
)
    {
    int x = get_global_id(0);
    int y = get_global_id(1);

    int padded_width = w + 2;

    outputImageL[(y + 1) * padded_width + (x + 1)] = inputImageL[y * w + x];
    outputImageR[(y + 1) * padded_width + (x + 1)] = inputImageR[y * w + x];
    }

__kernel void populate_integral_tables_row(
        __global uchar *inputImageL,
        __global uchar *inputImageR,
        __global uint *inputIntegralTempL,
        __global uint *inputIntegralTempR,
        __global ulong *inputIntegralSquaredTempL,
        __global ulong *inputIntegralSquaredTempR,
        int img_w,
        int img_h
        )
{
    int y = get_global_id(0);

    if (y >= img_h) return;

    unsigned int row_sumL = 0;
    unsigned int row_sumR = 0;
    unsigned long row_sum_squaredL = 0;
    unsigned long row_sum_squaredR = 0;

    for (int x = 0; x < img_w; x++)
    {
        row_sumL += inputImageL[y * img_w + x];
        row_sumR += inputImageR[y * img_w + x];

        ulong valL = (ulong)inputImageL[y*img_w+x];
        ulong valR = (ulong)inputImageR[y*img_w+x];

        row_sum_squaredL += valL * valL;
        row_sum_squaredR += valR * valR;

        inputIntegralTempL[y * img_w + x] = row_sumL;
        inputIntegralTempR[y * img_w + x] = row_sumR;
        inputIntegralSquaredTempL[y * img_w + x] = row_sum_squaredL;
        inputIntegralSquaredTempR[y * img_w + x] = row_sum_squaredR;
        
    }
}

__kernel void populate_integral_tables_col(
        __global uint *inputIntegralTempL,
        __global uint *inputIntegralTempR,
        __global ulong *inputIntegralSquaredTempL,
        __global ulong *inputIntegralSquaredTempR,
        __global uint *inputIntegralL,
        __global uint *inputIntegralR,
        __global ulong *inputIntegralSquaredL,
        __global ulong *inputIntegralSquaredR,
        int img_w,
        int img_h
        )
{
    int x = get_global_id(0);

    if (x >= img_w) return;

    unsigned int col_sumL = 0;
    unsigned int col_sumR = 0;
    unsigned long col_sum_squaredL = 0;
    unsigned long col_sum_squaredR = 0;

    for (int y = 0; y < img_h; y++)
    {
        col_sumL += inputIntegralTempL[y * img_w + x];
        col_sumR += inputIntegralTempR[y * img_w + x];
        col_sum_squaredL += inputIntegralSquaredTempL[y * img_w + x];
        col_sum_squaredR += inputIntegralSquaredTempR[y * img_w + x];

        inputIntegralL[y * img_w + x] = col_sumL;
        inputIntegralR[y * img_w + x] = col_sumR;
        inputIntegralSquaredL[y * img_w + x] = col_sum_squaredL;
        inputIntegralSquaredR[y * img_w + x] = col_sum_squaredR;
        
    }
}

__kernel void populate_mean_var_tables(
        __global uint *inputIntegralL,
        __global uint *inputIntegralR,
        __global ulong *inputIntegralSquaredL,
        __global ulong *inputIntegralSquaredR,
        __global float *meanTableL,
        __global float *meanTableR,
        __global float *varTableL,
        __global float *varTableR,
        int img_w,
        int img_h,
        int window
        )
{
    int x = get_global_id(0);
    int y = get_global_id(1);

    int n = window / 2;

    if(x < n + 1 || x >= img_w - n - 1 || y < n + 1 || y >= img_h - n - 1) return;

    int y_idx_min = y - n - 1;
    int x_idx_min = x - n - 1;
    int y_idx_max = y + n;
    int x_idx_max = x + n;


    uint S0_0L = inputIntegralL[y_idx_min * img_w + x_idx_min];
    uint S2_2L = inputIntegralL[y_idx_max * img_w + x_idx_max];
    uint S2_0L = inputIntegralL[y_idx_max * img_w + x_idx_min];
    uint S0_2L = inputIntegralL[y_idx_min * img_w + x_idx_max];
    ulong SQ0_0L = inputIntegralSquaredL[y_idx_min * img_w + x_idx_min];
    ulong SQ2_2L = inputIntegralSquaredL[y_idx_max * img_w + x_idx_max];
    ulong SQ2_0L = inputIntegralSquaredL[y_idx_max * img_w + x_idx_min];
    ulong SQ0_2L = inputIntegralSquaredL[y_idx_min * img_w + x_idx_max];

    uint S0_0R = inputIntegralR[y_idx_min * img_w + x_idx_min];
    uint S2_2R = inputIntegralR[y_idx_max * img_w + x_idx_max];
    uint S2_0R = inputIntegralR[y_idx_max * img_w + x_idx_min];
    uint S0_2R = inputIntegralR[y_idx_min * img_w + x_idx_max];
    ulong SQ0_0R = inputIntegralSquaredR[y_idx_min * img_w + x_idx_min];
    ulong SQ2_2R = inputIntegralSquaredR[y_idx_max * img_w + x_idx_max];
    ulong SQ2_0R = inputIntegralSquaredR[y_idx_max * img_w + x_idx_min];
    ulong SQ0_2R = inputIntegralSquaredR[y_idx_min * img_w + x_idx_max];

    float sumL = (float)(S2_2L + S0_0L - S2_0L - S0_2L);
    float sumSqL = (float)(SQ2_2L + SQ0_0L - SQ2_0L - SQ0_2L);
    float sumR = (float)(S2_2R + S0_0R - S2_0R - S0_2R);
    float sumSqR = (float)(SQ2_2R + SQ0_0R - SQ2_0R - SQ0_2R);

    meanTableL[y * img_w + x] = sumL / window / window;
    meanTableR[y * img_w + x] = sumR / window / window;

    varTableL[y * img_w + x] = sumSqL / window / window - meanTableL[y * img_w + x] * meanTableL[y * img_w + x];
    varTableR[y * img_w + x] = sumSqR / window / window - meanTableR[y * img_w + x] * meanTableR[y * img_w + x];

}
