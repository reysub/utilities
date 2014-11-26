/*
 * Read from a binary file a vector X(dim n*1) and a matrix A(dim n*n)
 * and compute X*A*X(transp)
 *
 * NOTE: the whole A does not fit in memory
 *
 * ASSUMPTION:
 *  - a process per row
 *	- vector X fits in memory
 *
 * inputFile's structure:
 * line 1 = double indicating the size
 * line 2 = vector x
 * line 3 and others = matrix A line by line
 *
 */

#include <stdio.h>
#include <mpi.h>
#include <stdlib.h>

#define MASTER 0
#define InputFile "/home/lorenzo/Desktop/Programmazione/Workspace/C - C++/C_CPD_1_VectorProduct/data/input.bin"

int main(int argc, char **argv){

	// common variable declaration
	int processID, sizeA;
	double partialResult;
	double* vectorX;
	double* lineOfMatrixA;

	// init MPI's environment
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &processID);


	// MASTER's work
	if (processID==MASTER){

		// variable declaration
		int i, numberOfProcesses;
		MPI_Comm_size(MPI_COMM_WORLD, &numberOfProcesses);

		// write something in the input file
		FILE *fp=fopen(InputFile, "wb");
		if(fp!=NULL){
			double d[4][3]={{1, 2, 3}, {4, 5, 6}, {7, 8, 9}, {10, 11, 12}};
			int d2=3;
			fwrite(&d2, sizeof(int), 1, fp);
			fwrite(d[0], sizeof(double), 3, fp);
			fwrite(d[1], sizeof(double), 3, fp);
			fwrite(d[2], sizeof(double), 3, fp);
			fwrite(d[3], sizeof(double), 3, fp);
			fclose(fp);
		}


		// read from binary file the size of A
		FILE *filePointer=fopen(InputFile, "rb");
		if (filePointer==NULL){
			printf("error while opening the file\n");
			return 1;
		}

		fread(&sizeA, sizeof(int), 1, filePointer);
		if (sizeA!=numberOfProcesses){
			printf("number of processes is different form the size of matrix A\n");
			return 1;
		}

		MPI_Bcast(&sizeA, 1, MPI_INT, MASTER, MPI_COMM_WORLD);
		printf("sent sizeA=%d\n", sizeA);

		// read the vector (read sizeA doubles)
		vectorX=(double*)malloc(sizeof(double)*sizeA);
		fread(vectorX, sizeof(double), sizeA, filePointer);
		MPI_Bcast(vectorX, sizeA, MPI_DOUBLE, MASTER, MPI_COMM_WORLD);
		printf("sent vectorX=[%lf, %lf, %lf]\n", vectorX[0], vectorX[1], vectorX[2]);


		for (i=1; i<sizeA; i++){

			// read a line of A and send it
			lineOfMatrixA=(double*)malloc(sizeof(double)*sizeA);
			fread(lineOfMatrixA, sizeof(double), sizeA, filePointer);
			MPI_Send(lineOfMatrixA, sizeA, MPI_DOUBLE, i, 3, MPI_COMM_WORLD);
			printf("master has sent to slave %d the line=[%lf, %lf, %lf]\n", i, lineOfMatrixA[0], lineOfMatrixA[1], lineOfMatrixA[2]);
			free(lineOfMatrixA);
		}

		// finally MASTER reads its part
		lineOfMatrixA=(double*)malloc(sizeof(double)*sizeA);
		fread(lineOfMatrixA, sizeof(double), sizeA, filePointer);
		printf("master has read the line=[%lf, %lf, %lf]\n", lineOfMatrixA[0], lineOfMatrixA[1], lineOfMatrixA[2]);

		// close the input file
		fclose(filePointer);
	}


	// SLAVES' WORK
	else {

		// receive data from master
		MPI_Bcast(&sizeA, 1, MPI_INT, MASTER, MPI_COMM_WORLD);



			vectorX=(double*)malloc(sizeof(double)*sizeA);
			MPI_Bcast(vectorX, sizeA, MPI_DOUBLE, MASTER, MPI_COMM_WORLD);

			lineOfMatrixA=(double*)malloc(sizeof(double)*sizeA);
			MPI_Recv(lineOfMatrixA, sizeA, MPI_DOUBLE, MASTER, 3, MPI_COMM_WORLD, NULL);

	}

	// COMMON WORK

		int i;
		for (i=0; i<sizeA; i++)
			partialResult+=vectorX[i]*lineOfMatrixA[i];

		partialResult*=vectorX[(processID+sizeA-1)%sizeA];

		printf("slave %d has computed value %lf\n", processID, partialResult);


		// final reduction for computing the result
		double result;


		MPI_Reduce(&partialResult, &result, 1, MPI_DOUBLE, MPI_SUM, MASTER, MPI_COMM_WORLD);


		// free memory

		free(vectorX); free(lineOfMatrixA);

		if (processID==MASTER)
			printf("the final result is: %lf\n", result);


	MPI_Finalize();

	return 0;
}
