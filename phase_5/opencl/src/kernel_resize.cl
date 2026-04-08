__kernel void resize_downsample4(
    __global const uchar* input,
    __global uchar* output,
    const int width,
    const int height)
{
    int x = get_global_id(0);
    int y = get_global_id(1);

    int new_width = width / 4;

    int src_x = x * 4;
    int src_y = y * 4;

    int dst_idx = (y * new_width + x) * 4;

    uint sum0 = 0;
    uint sum1 = 0;
    uint sum2 = 0;
    uint sum3 = 0;

    for(int oy = 0; oy < 4; oy++)
    {
        for(int ox = 0; ox < 4; ox++)
        {
            int sample_idx = ((src_y + oy) * width + (src_x + ox)) * 4;
            sum0 += input[sample_idx + 0];
            sum1 += input[sample_idx + 1];
            sum2 += input[sample_idx + 2];
            sum3 += input[sample_idx + 3];
        }
    }

    output[dst_idx + 0] = (uchar)(sum0 / 16);
    output[dst_idx + 1] = (uchar)(sum1 / 16);
    output[dst_idx + 2] = (uchar)(sum2 / 16);
    output[dst_idx + 3] = (uchar)(sum3 / 16);
}
