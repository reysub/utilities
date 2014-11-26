/*
 *
 * Il MASTER lavora e stampa il risultato a schermo alla fine
 *
 * File System condiviso
 *
 * Matrice A = grande
 * Matrice B = piccola
 *
 */

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#define MASTER 0

void fill(char*, char*);

int main(int argc, char **argv){ // inputA, inputB

	int processId, numberOfProcesses;
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &processId);
	MPI_Comm_size(MPI_COMM_WORLD, &numberOfProcesses);

	if (argc!=3 && processId==MASTER){
		printf("Error in number of parameters\n");
		MPI_Abort(MPI_COMM_WORLD, 0);
	}

	if (processId==MASTER)
		fill(argv[1], argv[2]);


	/****************************************************** MATRICE A **************************************************/

	FILE *inputFilePtr = fopen(argv[1], "rb");

	if (inputFilePtr==NULL && processId==MASTER){
		printf("Error while opening matrix A\n");
		MPI_Abort(MPI_COMM_WORLD, 0);
	}

	int rowsA, rowsB, columnsA, columnsB;
	fread(&rowsA, 1, sizeof(int), inputFilePtr);
	fread(&columnsA, 1, sizeof(int), inputFilePtr);
	printf("proc %d row=%d, col=%d\n", processId, rowsA, columnsA);

	int rowsAPerProcess = rowsA / numberOfProcesses;
	int rest = rowsA % numberOfProcesses;

	int startingLine = processId*rowsAPerProcess;
	startingLine += (processId<rest) ? processId : rest;

	rowsAPerProcess += (processId<rest) ? 1 : 0;

	printf("proc %d rpp=%d, start=%d\n", processId, rowsAPerProcess, startingLine);

	double *chunkA = malloc (rowsAPerProcess * columnsA * sizeof(double));

	fseek(inputFilePtr, startingLine * columnsA * sizeof(double) + 2 * sizeof(int), SEEK_SET);
	fread(chunkA, rowsAPerProcess * columnsA, sizeof(double), inputFilePtr);
	fclose (inputFilePtr);


	/****************************************************** MATRICE B **************************************************/

	inputFilePtr = fopen(argv[2], "rb");

	if (inputFilePtr==NULL && processId==MASTER){
		printf("Error while opening matrix B\n");
		MPI_Abort(MPI_COMM_WORLD, 0);
	}

	fread(&rowsB, 1, sizeof(int), inputFilePtr);
	fread(&columnsB, 1, sizeof(int), inputFilePtr);

	double *chunkB = malloc (rowsB * columnsB * sizeof(double));

	fread(chunkB, rowsB * columnsB, sizeof(double), inputFilePtr);
	fclose (inputFilePtr);


	/****************************************************** PRODOTTO **************************************************/

	double *result = malloc (rowsB * columnsB * rowsAPerProcess * columnsA * sizeof(double));

	for (int i = 0; i < rowsAPerProcess * columnsA; ++i)
		for (int j = 0; j < rowsB * columnsB; ++j)
			//     salto dei blocchi grandi                         salto delle righe                  salto delle colonne        // salto ultima colonna
			result[(i/columnsA) * columnsA * rowsB * columnsB   +   (j/columnsB) * rowsA * rowsB   +   (i%columnsA) * rowsB   +   j%columnsB] = chunkA[i] * chunkB[j];


	/****************************************************** STAMPA **************************************************/

	if (processId==MASTER){

		for (int j = 0; j < rowsB * columnsB * rowsAPerProcess * columnsA; ++j) {
			printf("%f ", result[j]);
			if ((j+1) % (rowsA*rowsB) == 0)
				printf("\n");
		}

		int chunkSize;
		for (int i = 1; i < numberOfProcesses; ++i) {
			MPI_Recv(&chunkSize, 1, MPI_INT, i, 0, MPI_COMM_WORLD, NULL);

			result = realloc(result, chunkSize * sizeof(double));
			MPI_Recv(result, chunkSize, MPI_DOUBLE, i, 1, MPI_COMM_WORLD, NULL);

			for (int j = 0; j < chunkSize ; ++j) {
				printf("%f ", result[j]);
				if ((j+1) % (rowsA*rowsB) == 0)
					printf("\n");
			}
		}
		printf("\n");
	} else {
		int chunkSize = rowsB * columnsB * rowsAPerProcess * columnsA;
		MPI_Send(&chunkSize, 1, MPI_INT, MASTER, 0, MPI_COMM_WORLD);
		MPI_Send(result, chunkSize, MPI_DOUBLE, MASTER, 1, MPI_COMM_WORLD);
	}

	free(result); free(chunkA); free(chunkB);
	MPI_Finalize();
	return 1;
}




void fill(char *path1, char *path2){
	FILE *f1 = fopen(path1, "wb");

	int a=3;
	fwrite(&a, sizeof(int), 1, f1);
	fwrite(&a, sizeof(int), 1, f1);

	double d1[3][3]={{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
	fwrite(d1[0], sizeof(double), 3, f1);
	fwrite(d1[1], sizeof(double), 3, f1);
	fwrite(d1[2], sizeof(double), 3, f1);
	fclose(f1);


	FILE *f2 = fopen(path2, "wb");

	a=2;
	fwrite(&a, sizeof(int), 1, f2);
	fwrite(&a, sizeof(int), 1, f2);

	double d2[2][2]={{1, 2}, {3, 4}};
	fwrite(d2[0], sizeof(double), 2, f2);
	fwrite(d2[1], sizeof(double), 2, f2);
	fclose(f2);
}

