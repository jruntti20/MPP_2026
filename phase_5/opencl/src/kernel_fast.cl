#define MAX_DISPARITY 256
#define WINDOW 5

__kernel void zncc_fast(
    __global uchar* left,
    __global uchar* right,
    __global uchar* disparity,
    int disp_sign,
    int width,
    int height)
{
    int x = get_global_id(0);
    int y = get_global_id(1);

    if(x < WINDOW || y < WINDOW || x >= width-WINDOW || y >= height-WINDOW)
    {
        disparity[y*width+x] = 0;
        return;
    }

    int best_d = 0;
    float best_score = -1.0f;

    for(int d=0; d<MAX_DISPARITY && x+d < width-WINDOW; d++)
    {
        float sumL=0,sumR=0,sumLR=0,sumL2=0,sumR2=0;
        int count=0;

        for(int wy=-WINDOW; wy<=WINDOW; wy++)
        for(int wx=-WINDOW; wx<=WINDOW; wx++)
        {
            int xl=x+wx;
            int yl=y+wy;
            int xr=xl-(d*disp_sign);

            int idxL=((yl*width+xl)*4);
            int idxR=((yl*width+xr)*4);

            float L =
            0.299f*left[idxL] +
            0.587f*left[idxL+1] +
            0.114f*left[idxL+2];

            float R =
            0.299f*right[idxR] +
            0.587f*right[idxR+1] +
            0.114f*right[idxR+2];

            sumL+=L;
            sumR+=R;
            sumLR+=L*R;
            sumL2+=L*L;
            sumR2+=R*R;

            count++;
        }

        float numerator = count*sumLR - sumL*sumR;
        float denomL = count*sumL2 - sumL*sumL;
        float denomR = count*sumR2 - sumR*sumR;

        if(denomL < 1e-5f || denomR < 1e-5f)
            continue;

        float score = numerator / sqrt(denomL * denomR);

        if(score > best_score)
        {
            best_score = score;
            best_d = d;
        }
    }

    disparity[y*width+x] = abs(best_d) * 255 / MAX_DISPARITY;
}
