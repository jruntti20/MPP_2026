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

    int src_idx = (src_y * width + src_x) * 4;
    int dst_idx = (y * new_width + x) * 4;

    // Copy RGBA
    output[dst_idx + 0] = input[src_idx + 0];
    output[dst_idx + 1] = input[src_idx + 1];
    output[dst_idx + 2] = input[src_idx + 2];
    output[dst_idx + 3] = input[src_idx + 3];
}