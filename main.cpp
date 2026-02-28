#include <stdlib.h>
#include <iostream>
#include "mat_mul.h"
#include <bits/stdc++.h>


#define ROWS1 2
#define COLS1 4
#define ROWS2 4
#define COLS2 2



int main(int argc, char *argv[])
{
    int mat1_rows = 0;
    int mat1_cols = 0;
    int mat2_rows = 0;
    int mat2_cols = 0;

    // Parse arguments
    std::cout << "Type row count for mat1: " << std::endl;
    std::cin >> mat1_rows;
    std::cout << "Type col count for mat1: " << std::endl;
    std::cin >> mat1_cols;
    int sizeMat1 = mat1_rows * mat1_cols;

    std::cout << "Type row count for mat2: " << std::endl;
    std::cin >> mat2_rows;
    std::cout << "Type col count for mat2: " << std::endl;
    std::cin >> mat2_cols;
    int sizeMat2 = mat2_rows * mat2_cols;

    if (mat1_cols != mat2_rows)
    {
        std::cout << "Matrix dimensions do not match for matrix multiplication!" << std::endl;
        exit(1);
    }

    int sizeResMat = mat1_rows * mat2_cols;

    struct Matrix2d* mat1 = (struct Matrix2d*)std::malloc(sizeof(Matrix2d)); 
    mat1->data = (int *)malloc(sizeMat1 * sizeof(int));
    if(mat1->data == NULL)
    {
        free(mat1);
    }
    struct Matrix2d* mat2 = (struct Matrix2d*)std::malloc(sizeof(Matrix2d) + sizeMat2 * sizeof(int));
    mat2->data = (int *)malloc(sizeMat2 * sizeof(int));
    if(mat2->data == NULL)
    {
        free(mat2);
    }

    struct Matrix2d* resMat = (struct Matrix2d*)std::malloc(sizeof(Matrix2d) + sizeResMat * sizeof(int));
    resMat->data = (int *)malloc(sizeResMat * sizeof(int));
    if(resMat->data == NULL)
    {
        free(resMat);
    }


    mat1->cols = mat1_cols;
    mat1->rows = mat1_rows;
    mat2->cols = mat2_cols;
    mat2->rows = mat2_rows;


    resMat->rows = mat1_rows;
    resMat->cols = mat2_cols;

    std::cout << "The resulting matrix will be a " << mat1_rows << " x " << mat2_cols << " sized matrix." << std::endl; 
    std::cout << "Please provide " << sizeMat1 << " values for the input matrix 1." << std::endl;
    for(int i = 0; i < sizeMat1; i++)
    {
        std::cin >> mat1->data[i];
    }
    std::cout << "Please provide " << sizeMat2 << " values for the input matrix 2." << std::endl;
    for(int i = 0; i < sizeMat2; i++)
    {
        std::cin >> mat2->data[i];
    }

    for(int i = 0; i < sizeResMat; i++)
    {
        resMat->data[i] = 0;
    }

    /*
    int data1[8] = {1,1,1,1,2,2,2,2};
    mat1->data = data1;
    int data2[8] = {1,2,1,2,1,2,1,2};
    mat2->data = data2;
    int dataRes[4] = {0,0,0,0};
    resMat->data = dataRes;
    */

    bool ret = mat_mul(resMat, mat1, mat2);

    if(ret)
    {
        std::cout << "Printing out result matrix..." << std::endl;
        for (int i = 0; i < resMat->cols; i++)
        {
            for(int j = 0; j < resMat->rows; j++)
            {
                std::cout << resMat->data[i * resMat->cols + j] << " ";
            }
            std::cout << std::endl;
        }
    }
    else
    {
        std::cout << "And error occured in program execution..." << std::endl;
    }

    free(mat1);
    free(mat2);
    free(resMat);

    return 0;

    
}

