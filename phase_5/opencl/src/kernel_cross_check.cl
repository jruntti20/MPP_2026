

__kernel void cross_check(
    __global const uchar* dispL,
    __global const uchar* dispR,
    __global uchar* output,
    const int threshold,
    const int max_disparity,
    const int width,
    const int height)
{
    int x = get_global_id(0);
    int y = get_global_id(1);

    if(x >= width || y >= height)
        return;

    int idx = y * width + x;
    uchar dL = dispL[idx];
    int dispLPixels = ((int)dL * max_disparity + 127) / 255;

    int xr = x - dispLPixels;
    if(xr < 0 || xr >= width)
    {
        output[idx] = 0;
        return;
    }

    uchar dR = dispR[y * width + xr];
    int dispRPixels = ((int)dR * max_disparity + 127) / 255;

    if(abs(dispLPixels - dispRPixels) > threshold)
        output[idx] = 0;
    else
        output[idx] = dL;
}
