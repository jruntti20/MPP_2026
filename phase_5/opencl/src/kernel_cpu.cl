__kernel void zncc_cpu(
    __global uchar* img1,
    __global uchar* img2,
    __global uchar* output,
    int width,
    int height,
    int win_size,
    int min_d,
    int max_d)
{
    int u = get_global_id(1);
    int v = get_global_id(0);

    if(u >= height || v >= width)
        return;

    int n = win_size/2;

    if(u < n || u >= height-n || v < n || v >= width-n)
    {
        output[u*width+v] = 0;
        return;
    }

    float best_score=-1.0f;
    int best_disp=0;

    float avg1=0;
    int count=0;

    for(int i=-n;i<=n;i++)
    for(int j=-n;j<=n;j++)
    {
        avg1 += img1[((u+i)*width+(v+j))*4];
        count++;
    }

    avg1/=count;

    for(int di=min_d; di<=max_d; di++)
    {
        if(v-di < n || v-di >= width-n)
            continue;

        float avg2=0;
        count=0;

        for(int i=-n;i<=n;i++)
        for(int j=-n;j<=n;j++)
        {
            avg2 += img2[((u+i)*width+(v+j-di))*4];
            count++;
        }

        avg2/=count;

        float upper=0;
        float lower0=0;
        float lower1=0;

        for(int i=-n;i<=n;i++)
        for(int j=-n;j<=n;j++)
        {
            int p1=((u+i)*width+(v+j))*4;
            int p2=((u+i)*width+(v+j-di))*4;

            float ld=img1[p1]-avg1;
            float rd=img2[p2]-avg2;

            upper+=ld*rd;
            lower0+=ld*ld;
            lower1+=rd*rd;
        }

        float denom=sqrt(lower0*lower1);

        if(denom<=1e-5f)
            continue;

        float score=upper/denom;

        if(score>best_score)
        {
            best_score=score;
            best_disp=abs(di);
        }
    }

    output[u*width+v]=best_disp*255/(max_d-min_d);
}