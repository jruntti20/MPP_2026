#include <stdbool.h>

typedef struct Matrix2d
{
    int rows;
    int cols;
    int *data;
} Matrix2d;

bool mat_mul(Matrix2d* resMat, Matrix2d* mat1, Matrix2d *mat2);


