/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 * 
 * Name: Sivan ELisha
 * email: e.sivan@wustl.edu
 */ 
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
/**
 * transpose_submit - optimized matrix transpose with minimal cache misses
 * this function is optimized for 32x32, 64x64 matrices and a general case for other sizes
 * it uses blocking to enhance the cache hit rate and handles diagonal elements to avoid extra misses
 *
 * @param M the number of columns in the matrix A and rows in B
 * @param N the number of rows in the matrix A and columns in B
 * @param A the source matrix
 * @param B the destination matrix for the transpose
 */
void transpose_submit(int M, int N, int A[N][M], int B[M][N]) {
    int blockSize; // block size for submatrix traversal
    int blkRow, blkCol; // iterators for block rows and columns
    int row, col; // iterators within individual blocks
    int diagonalIndex; // index for handling diagonal elements
    int temp; // temporary variable for swapping elements

    // handle 32x32 matrices
    if (N == 32) {
        blockSize = 8; // optimal block size determined empirically

        for (blkCol = 0; blkCol < N; blkCol += blockSize) {
            for (blkRow = 0; blkRow < N; blkRow += blockSize) {
                for (row = blkRow; row < blkRow + blockSize; row++) {
                    for (col = blkCol; col < blkCol + blockSize; col++) {
                        // handle diagonal elements separately to avoid conflict misses
                        if (row == col) {
                            temp = A[row][col];
                            diagonalIndex = row;
                        }
                        else {
                            B[col][row] = A[row][col];
                        }
                    }
                    // re-assign the diagonal element stored in temp
                    if (blkCol == blkRow) {
                        B[diagonalIndex][diagonalIndex] = temp;
                    }
                }
            }
        }
    }
    else if (N == 64) {
        // special handling for 64x64 matrices to reduce cache misses
        blockSize = 4; // smaller block size for this case

        int temp0, temp1, temp2, temp3, temp4;

        for (row = 0; row < N; row += blockSize) {
            for (col = 0; col < M; col += blockSize) {
                // temporarily store elements from matrix A to reduce misses
                temp0 = A[row][col];
                temp1 = A[row + 1][col];
                temp2 = A[row + 2][col];
                temp3 = A[row + 2][col + 1];
                temp4 = A[row + 2][col + 2];

                // reassign elements in B with a strategy to minimize misses
                B[col + 3][row] = A[row][col + 3];
                B[col + 3][row + 1] = A[row + 1][col + 3];
                B[col + 3][row + 2] = A[row + 2][col + 3];

                B[col + 2][row] = A[row][col + 2];
                B[col + 2][row + 1] = A[row + 1][col + 2];
                B[col + 2][row + 2] = temp4;
                temp4 = A[row + 1][col + 1];

                B[col + 1][row] = A[row][col + 1];
                B[col + 1][row + 1] = temp4;
                B[col + 1][row + 2] = temp3;

                B[col][row] = temp0;
                B[col][row + 1] = temp1;
                B[col][row + 2] = temp2;

                B[col][row + 3] = A[row + 3][col];
                B[col + 1][row + 3] = A[row + 3][col + 1];
                B[col + 2][row + 3] = A[row + 3][col + 2];
                temp0 = A[row + 3][col + 3];

                B[col + 3][row + 3] = temp0;
            }
        }
    }
    else {
        // general case for other matrix sizes
        blockSize = 16; // a general block size for large matrices

        for (blkCol = 0; blkCol < M; blkCol += blockSize) {
            for (blkRow = 0; blkRow < N; blkRow += blockSize) {
                // iterate within blocks, ensuring not to exceed matrix dimensions
                for (row = blkRow; (row < blkRow + blockSize) && (row < N); row++) {
                    for (col = blkCol; (col < blkCol + blockSize) && (col < M); col++) {
                        // diagonal elements handled separately to avoid extra misses
                        if (row == col) {
                            temp = A[row][col];
                            diagonalIndex = row;
                        }
                        else {
                            B[col][row] = A[row][col];
                        }
                    }
                    // re-assign diagonal elements stored in temp
                    if (blkRow == blkCol) {
                        B[diagonalIndex][diagonalIndex] = temp;
                    }
                }
            }
        }
    }
}


/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc); 

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

