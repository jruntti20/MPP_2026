__kernel void resizeImage(
    __global const uchar4* input,
    __global uchar4* output,
    int width,
    int height)
{
    int x = get_global_id(0);
    int y = get_global_id(1);

    int newWidth = width / 4;

    int srcX = x * 4;
    int srcY = y * 4;

    int srcIndex = srcY * width + srcX;
    int dstIndex = y * newWidth + x;

    output[dstIndex] = input[srcIndex];
}


__kernel void grayscale(
    __global const uchar* input, // 4 bytes per pixel
    __global uchar* output,
    int width)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    int index = y * width + x;
    int src = index * 4;
    float Y = 0.2126f * input[src+0] +
              0.7152f * input[src+1] +
              0.0722f * input[src+2];
    output[index] = (uchar)Y;
}

__kernel void filter5x5(
    __global const uchar* input,
    __global uchar* output,
    int width,
    int height)
{
    int x = get_global_id(0);
    int y = get_global_id(1);

    int sum = 0;
    int count = 0;

    for(int dy=-2; dy<=2; dy++)
    {
        for(int dx=-2; dx<=2; dx++)
        {
            int nx = x + dx;
            int ny = y + dy;

            if(nx>=0 && ny>=0 && nx<width && ny<height)
            {
                sum += input[ny*width + nx];
                count++;
            }
        }
    }

    output[y*width + x] = sum / count;
}