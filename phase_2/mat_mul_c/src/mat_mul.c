#include "mat_mul.h"
#include "stdlib.h"


bool mat_mul(Matrix2d *resMat, Matrix2d *mat1, Matrix2d *mat2)
{
   int cols1 = mat1->cols;
   int rows2 = mat2->rows;

   if (cols1 != rows2)
   {
       return false;
   }
   else
   {
       for (int y = 0; y < mat1->rows; y++)
       {
           for(int x = 0; x < mat2->cols; x++)
           {
               int cellVal = 0;

               for (int j = 0; j < mat1->cols; j++)
               {
                   int val1 = mat1->data[y * mat1->cols + j];
                   int val2 = mat2->data[j * mat2->cols + x];
                   cellVal += val1 * val2; 
               }
               resMat->data[y * mat1->rows + x] = cellVal;
           }
       }
       return true;
   }

}

Matrix2d *transposeMatrix(Matrix2d *mat)
{
    struct Matrix2d *transposeMat = (struct Matrix2d*)malloc(sizeof(Matrix2d));

    transposeMat->rows = mat->cols;
    transposeMat->cols = mat->rows;
    transposeMat->data = (int *)calloc(transposeMat->rows * transposeMat->cols, sizeof(int));

    for(int i = 0; i < mat->rows; i++)
    {
        for (int j = 0; j < mat->cols; j++)
        {
            transposeMat->data[j * mat->rows + i] = mat->data[i * mat->cols + j];
        }
    }
    return transposeMat;

}

bool mat_mul_transpose(Matrix2d *resMat, Matrix2d *mat1, Matrix2d *mat2)
{
    if (mat1->cols != mat2->rows)
    {
        return false;
    }
    else
    {
        Matrix2d *mat2_trans = transposeMatrix(mat2);

        for(int i = 0; i < mat1->rows; i++)
        {
            for(int j = 0; j < mat2->cols; j++)
            {
                int sum = 0;
                for (int k = 0; k < mat1->cols; k++)
                {
                    sum += mat1->data[i * mat1->cols + k] * mat2_trans->data[j * mat2->cols + k];
                }
                resMat->data[i * mat2->cols + j] = sum;
            }
        }
        free(mat2_trans);
        return true;
    }
}



