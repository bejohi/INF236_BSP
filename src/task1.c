#include "task1.h"


#define DEBUG 0
#define DEEP_DEBUG 0
#define REPORT_MODE 0

static unsigned int numberOfProcessors;
static int globalN;
static int globalI;
static int globalJ;


/**
 * This method will run on every machine.
 */
void matrixMultOldFashion() {

    bsp_begin(numberOfProcessors);

    int p = bsp_nprocs();
    int processorId = bsp_pid();
    int n = globalN;
    int iToCheck = globalI;
    int jToCheck = globalJ;

    // Distribution
    bsp_push_reg(&n, sizeof(long));
    bsp_push_reg(&iToCheck, sizeof(int));
    bsp_push_reg(&jToCheck, sizeof(int));
    bsp_sync();

    bsp_get(0, &n, 0, &n, sizeof(long));
    bsp_get(0, &iToCheck, 0, &iToCheck, sizeof(int));
    bsp_get(0, &jToCheck, 0, &jToCheck, sizeof(int));
    bsp_sync();
    bsp_pop_reg(&n);
    bsp_pop_reg(&iToCheck);
    bsp_pop_reg(&jToCheck);

    int start = n / p * processorId;
    int end = n / p * (processorId + 1);
    int k = start;
    int nrows = end - start;  // == n/p

    // Matrix init
    double *pointerA = (double *) malloc(sizeof(double) * n * nrows);
    double *pointerB = (double *) malloc(sizeof(double) * n * nrows);
    double *pointerC = (double *) malloc(sizeof(double) * n * nrows);
    double **matrixA = (double **) malloc(sizeof(double *) * nrows);
    double **matrixB = (double **) malloc(sizeof(double *) * nrows);
    double **localMatrixC = (double **) malloc(sizeof(double *) * nrows);
    double *iRow = (double *) malloc(sizeof(double) * n);
    double *jColum = (double *) malloc(sizeof(double) * n);

    for (int i = 0; i < nrows; i++) {
        matrixA[i] = pointerA + i * n;
        matrixB[i] = pointerB + i * n;
        localMatrixC[i] = pointerC + i * n;
    }

    srand((unsigned int) (time(NULL) * processorId));
    for (int i = 0; i < nrows; i++) {
        for (int y = 0; y < n; y++) {
            matrixA[i][y] = (double) rand() / (double) (RAND_MAX);
            matrixB[i][y] = (double) rand() / (double) (RAND_MAX);
            localMatrixC[i][y] = 0;
        }
    }

    bsp_push_reg(pointerA, n * nrows * sizeof(double));
    bsp_push_reg(pointerB, n * nrows * sizeof(double));
    bsp_push_reg(pointerC, n * nrows * sizeof(double));
    bsp_sync();

    int iLocal = iToCheck / nrows;
    int iRemote = iToCheck % nrows;

    // Collect the i-th row and j-colum
    bsp_get(iLocal, pointerA, iRemote * sizeof(double) * n, iRow, n * sizeof(double));
    bsp_sync();

    for (int localP = 0; localP < p; localP++) {
        for (int localN = 0; localN < nrows; localN++) {
            bsp_get(localP, pointerB, (localN * n + jToCheck) * sizeof(double), jColum + localP * nrows + localN,
                    sizeof(double));
            bsp_sync();
        }
    }

    if (DEBUG) printf("...Matrix init done for processorId=%d\n", processorId);

    // Algorithm begin
    bsp_sync();
    double timeStart = bsp_time();
    do {
        for (int i = 0; i < nrows; i++) {
            for (int h = k; h < k + n / p; h++) { // h or k
                for (int j = 0; j < n; j++) {
                    if (DEEP_DEBUG) {
                        printf("i=%d,j=%d,h=%d,k=%d for processorId=%d\n", i, j, h, k, processorId);
                    }
                    localMatrixC[i][j] += matrixA[i][h] * matrixB[h - k][j];
                }
            }
        }
        k = (k + nrows) % n;


        if (DEBUG) printf("Start distribution k=%d for processorId=%d...\n", k, processorId);

        // %p to shift the matrix to processor 0 in case processorId+1 == p
        bsp_get((processorId + 1) % p, pointerB, 0, pointerB, n * nrows * sizeof(double));
        bsp_sync();
        if (DEBUG) printf("...distribution k=%d for processorId=%d done...\n", k, processorId);
    } while (k != start);


    bsp_sync();
    double timeEnd = bsp_time();

    if (DEBUG) printf("...calculations done for processorId=%d\n", processorId);


    bsp_sync();
    if (processorId == 0) {
        printf("...calculations done in %.6lf seconds\n", timeEnd - timeStart);
    }

    // Verify result
    double sequ_result = 0;
    double result = 0;

    if (processorId == 0) {
        for (int x = 0; x < n; x++) {
            if (!REPORT_MODE) printf("iRow[%d]=%lf jColum[%d]=%lf\n", x, iRow[x], x, jColum[x]);
            sequ_result += iRow[x] * jColum[x];
        }
        bsp_get(iLocal, pointerC, ((iToCheck % nrows) * n + jToCheck) * sizeof(double), &result,
                sizeof(double)); // pid,source,offset,destination,size

    }

    bsp_sync();
    if (processorId == 0) {
        if (result != sequ_result) {
            printf("CHECK FAILED!\n");
            if (DEBUG) printf("Parallel result for (%d,%d)= %lf\n", iToCheck, jToCheck, result);
            if (DEBUG) printf("Sequ result=%lf\n", sequ_result);
        } else {
            printf("Check okay.\n");
        }
    }

    // Clean-Up
    free(pointerA);
    free(pointerB);
    free(pointerC);
    free(matrixA);
    free(matrixB);
    free(localMatrixC);
    free(iRow);
    free(jColum);

    bsp_end();
}

int main(int argc, char **argv) {
    bsp_init(matrixMultOldFashion, argc, argv);
    numberOfProcessors = 1;
    globalN = 1800;
    globalI = 5;
    globalJ = 4;

    printf("Please enter number of processors:");
    scanf("%d", &numberOfProcessors);

    if (!REPORT_MODE) {
        printf("\nPlease enter size of matrix:");
        scanf("%d", &globalN);

        printf("\nPlease enter i:");
        scanf("%d", &globalI);

        printf("\nPlease enter j:");
        scanf("%d", &globalJ);

        printf("\n");
    }

    if (globalI >= globalN) {
        globalI = globalN - 1;
    }
    if (globalJ >= globalN) {
        globalJ = globalN - 1;
    }

    if (globalN % numberOfProcessors != 0) {
        numberOfProcessors = 1;
    }

    if (numberOfProcessors > bsp_nprocs()) {
        numberOfProcessors = bsp_nprocs();
    }
    printf("Start program with n=%d,p=%d...\n", globalN, numberOfProcessors);
    matrixMultOldFashion();

    exit(EXIT_SUCCESS);
}