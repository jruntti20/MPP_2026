__kernel void occlusion_fill_opt(
    __global uchar* input,
    __global uchar* output,
    int width,
    int height,
    int range)
{
    int x = get_global_id(0);
    int y = get_global_id(1);

    if (x >= width || y >= height)
        return;

    int idx = y * width + x;
    uchar val = input[idx];

    if (val > 0)
    {
        output[idx] = val;
        return;
    }

    uchar best = 0;

    for (int r = 1; r <= range; r++)
    {
        int xl = x - r;
        if (xl >= 0)
        {
            uchar v = input[y * width + xl];
            if (v > 0)
            {
                best = v;
                break;
            }
        }

        int xr = x + r;
        if (xr < width)
        {
            uchar v = input[y * width + xr];
            if (v > 0)
            {
                best = v;
                break;
            }
        }
    }

    output[idx] = best;
}