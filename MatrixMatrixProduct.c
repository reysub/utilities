/*
 * SHARED FILE-SYSTEM, RAW-STORED MATRICES, MASTER JOINS THE COMPUTATION
 *
 */

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MASTER 0


int main(int argc, char **argv){ // argv1=inputA, argv2=inputB, argv3=Ra, argv4=Ca=Rb, argv5=Cb, argv6=output

	// Variable declaration
	int processId, numberOfProcesses;
	double *chunkMatrixA = NULL; double *chunkMatrixB = NULL; double *result = NULL;

	// MPI initialization
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &processId);
	MPI_Comm_size(MPI_COMM_WORLD, &numberOfProcesses);

	// check input parameters
	if (argc!=7)
		if (processId==MASTER) {
			printf("Error in numer of parameters\n");
			MPI_Abort(MPI_COMM_WORLD, 0);
		}

	int rowsA = atoi(argv[3]), columnsA = atoi(argv[4]), rowsB = atoi(argv[4]), columnsB = atoi(argv[5]);


	// ************************************************* MATRIX A *************************************************

	// open the input file containing matrix A
	FILE *inputPtrA = fopen(argv[1], "rb");
	if (inputPtrA==NULL)
		if (processId==MASTER) {
			printf("Error while opening matrix A\n");
			MPI_Abort(MPI_COMM_WORLD, 0);
		}

	// each process computes his part of work
	int rowsAPerProcess = rowsA / numberOfProcesses;
	int restA = rowsA % numberOfProcesses;

	// and the row it has to start read from
	int startingRow = processId * rowsAPerProcess;
	startingRow += (processId>restA) ? restA : processId;

	// considers the rest
	rowsAPerProcess += (processId<restA) ? 1 : 0;

	// allocates the memory
	chunkMatrixA = malloc(rowsAPerProcess * columnsA * sizeof(double));

	// sets the stream
	fseek(inputPtrA, startingRow*columnsA*sizeof(double), SEEK_SET);

	// and starts reading
	fread(chunkMatrixA, sizeof(double), rowsAPerProcess*columnsA, inputPtrA);

	// finally closes the input stream
	fclose(inputPtrA);


	// ************************************************* MATRIX B *************************************************

	// open the input file containing matrix A
	FILE *inputPtrB = fopen(argv[2], "rb");
	if (inputPtrB==NULL)
		if (processId==MASTER) {
			printf("Error while opening matrix B\n");
			MPI_Abort(MPI_COMM_WORLD, 0);
		}

	// each process computes his part of work
	int columnsBPerProcess = columnsB / numberOfProcesses;
	int restB = columnsB % numberOfProcesses;

	// and the row it has to start read from
	int startingColumn = processId * columnsBPerProcess;
	startingColumn += (processId>restB) ? restB : processId;

	// considers the rest
	columnsBPerProcess += (processId<restB) ? 1 : 0;

	// allocates the memory
	chunkMatrixB = malloc(columnsBPerProcess * rowsB * sizeof(double));


	for (int i = 0; i < columnsBPerProcess; ++i)
		for (int j = 0; j < rowsB; ++j) {
			// sets the stream
			fseek(inputPtrB, (j * columnsB + startingColumn + i) * sizeof(double), SEEK_SET);
			// and reads a value
			fread(&chunkMatrixB[i * rowsB + j], sizeof(double), 1, inputPtrB);
		}


	// finally closes the input stream
	fclose(inputPtrB);


	// ************************************************* MATRICES MULTIPLICATION *************************************************

	int vectorBSize = columnsBPerProcess*rowsB, vectorASize = rowsAPerProcess*columnsA, receiveFrom=processId;
	int lastUsedAPos=0;

	// allocate the memory for the final result vector
	result = malloc(rowsAPerProcess * columnsB * sizeof(double));
	memset(result, 0, rowsAPerProcess * columnsB * sizeof(double));


	for (int globalIterator = 0; globalIterator < numberOfProcesses; ++globalIterator) {

		for (int i = 0; i < columnsBPerProcess; ++i)
			for (int j = 0; j < rowsAPerProcess; ++j)
				for (int k = 0; k < rowsB; ++k)
					result[j * columnsB + i + startingColumn] += chunkMatrixA[j * columnsA + k] * chunkMatrixB[i * rowsB + k];

		// send data to the next process
		MPI_Send(&startingColumn, 1, MPI_INT, (processId+1)%numberOfProcesses, 0, MPI_COMM_WORLD);
		MPI_Send(&columnsBPerProcess, 1, MPI_INT, (processId+1)%numberOfProcesses, 1, MPI_COMM_WORLD);
		MPI_Send(chunkMatrixB, vectorBSize, MPI_DOUBLE, (processId+1)%numberOfProcesses, 2, MPI_COMM_WORLD);

		// receive data from the previous one
		receiveFrom = (processId==MASTER) ? numberOfProcesses-1 : processId-1;
		MPI_Recv(&startingColumn, 1, MPI_INT, receiveFrom, 0, MPI_COMM_WORLD, NULL);
		MPI_Recv(&columnsBPerProcess, 1, MPI_INT, receiveFrom, 1, MPI_COMM_WORLD, NULL);
		vectorBSize = columnsBPerProcess * rowsB;
		chunkMatrixB = realloc(chunkMatrixB, columnsBPerProcess * rowsB * sizeof(double));
		MPI_Recv(chunkMatrixB, vectorBSize, MPI_DOUBLE, receiveFrom, 2, MPI_COMM_WORLD, NULL);
	}


	// ************************************************* WRITE OUTPUT FILE *************************************************

	FILE *outputPtr;

	// MASTER'S WORK
	if (processId==MASTER){
		// open the output file
		outputPtr = fopen(argv[6], "wb");
		if (outputPtr==NULL) {
			printf("Error while opening the output file\n");
			MPI_Abort(MPI_COMM_WORLD, 0);
			return 1;
		}

		// write data and close the file
		fwrite(result, sizeof(double), columnsB*rowsAPerProcess, outputPtr);
		fclose(outputPtr);

		// send a signal to other processes
		int discard;
		for (int i = 1; i < numberOfProcesses; ++i) {
			MPI_Send(&i, 1, MPI_INT, i, 100, MPI_COMM_WORLD);
			MPI_Recv(&discard, 1, MPI_INT, i, 101, MPI_COMM_WORLD, NULL);
		}
	}

	// SLAVES' WORK
	else {
		int i = 0;
		// receive signal from master
		MPI_Recv(&i, 1, MPI_INT, MASTER, 100, MPI_COMM_WORLD, NULL);
		// append the output file
		outputPtr = fopen(argv[6], "ab");
		if (outputPtr==NULL) {
			printf("Error while opening the output file\n");
			MPI_Abort(MPI_COMM_WORLD, 0);
			return 1;
		}

		// write data
		fwrite(result, sizeof(double), columnsB*rowsAPerProcess, outputPtr);
		fclose(outputPtr);

		// send back a termination signal
		MPI_Send(&i, 1, MPI_INT, MASTER, 101, MPI_COMM_WORLD);
	}



	// ************************************************* CHECK *************************************************

	if (processId==MASTER){
		double val;
		outputPtr = fopen(argv[6], "rb");
		for (int i = 0; i < columnsB*rowsA; ++i){
			fread(&val, 1, sizeof(double), outputPtr);
			printf("%f ", val);
			if ((i+1)%columnsB==0)
				printf("\n");
		}
		fclose(outputPtr);
	}


	free(chunkMatrixA); free(chunkMatrixB); free(result);

	MPI_Finalize();
	return 0;
}
