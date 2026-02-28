#include "mat_mul.h"


bool mat_mul(Matrix2d *resMat, Matrix2d* mat1, Matrix2d* mat2)
{
   int cols1 = mat1->cols;
   int rows2 = mat2->rows;

   if (cols1 != rows2)
   {
       return false;
   }
   else
   {
       for (int x = 0; x < mat1->rows; x++)
       {
           for(int y = 0; y < mat2->cols; y++)
           {
               int cellVal = 0;

               for (int i = 0; i < mat1->cols; i++)
               {
                   int val1 = mat1->data[x * mat1->cols + i];
                   int val2 = mat2->data[i * mat2->cols + y];
                   cellVal += val1 * val2; 
               }
               resMat->data[x * mat1->rows + y] = cellVal;
           }
       }
       return true;
   }

}



