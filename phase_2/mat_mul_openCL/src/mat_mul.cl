

__kernel void mat_mul(__global float4 *A, __global float4 *B, __global float *C, int N)
{
    
    int num_rows = get_global_size(0);

    int vectors_in_row = num_rows / 4;

    int row = get_global_id(0);
    int col = get_global_id(1);

    int start = row * vectors_in_row;


    C[row * N + col] = dot(A[


}


__kernel void transpose(__global float4 *g_mat, __local float4 *l_mat, uint size)
{
    __global float4 *src, *dst;

    int col = get_global_id(0)
    int row = 0;
    while(col >= size) 
    {
        col -= size--;
        row++;
    }


}
