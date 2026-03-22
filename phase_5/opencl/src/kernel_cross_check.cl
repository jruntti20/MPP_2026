

__kernel void cross_check(
    __global const uchar* dispL,
    __global const uchar* dispR,
    __global uchar* output,
    const int threshold,
    const int width,
    const int height)
{
    int x = get_global_id(0);
    int y = get_global_id(1);

    if(x >= width || y >= height)
        return;

    int idx = y * width + x;
    uchar dL = dispL[idx];

    int xr = x - dL;
    if(xr < 0 || xr >= width)
    {
        output[idx] = 0;
        return;
    }

    uchar dR = dispR[y * width + xr];

    if(abs((int)dL - (int)dR) > threshold)
        output[idx] = 0;
    else
        output[idx] = dL;
}