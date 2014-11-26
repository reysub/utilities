/*
 * Implementation of distributed Prim's Algorithm
 *
 * INPUT FILE STRUCTURE
 * first line: integer K
 * other lines: a matrix of doubles of size K*K
 * each line of the file is a column of the matrix (that is, the matrix is saved transposed)
 *
 *
 * NOTE:
 *  - each process is assigned with a certain number n of columns, normally K/numOfProc (+ 1) in case of a rest
 *  - each process is assigned with a vector of all visited nodes (red-circled nodes according to the slides)
 *  - shared file-system, so each process can read his part of the matrix from the input file independently
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

#define INPUT_FILE "../data/input.bin"
#define MASTER 0
#define INITIAL_NODE 0
#define MAXIMUM_DOUBLE_VALUE 99999


int main (int argc, char **argv){

	int processId, numberOfProcesses, totalSize, chunkSize, rest;
	double **chunk; int *visited;

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &processId);
	MPI_Comm_size(MPI_COMM_WORLD, &numberOfProcesses);



		// write something in the input file
		if (processId==MASTER){
			int s=5;
			double v1[5]={0,1,9,6,MAXIMUM_DOUBLE_VALUE};
			double v2[5]={1, 0, MAXIMUM_DOUBLE_VALUE, 3, 4};
			double v3[5]={9, MAXIMUM_DOUBLE_VALUE, 0, 6, MAXIMUM_DOUBLE_VALUE};
			double v4[5]={6, 3, 6, 0, 8};
			double v5[5]={MAXIMUM_DOUBLE_VALUE, 4, MAXIMUM_DOUBLE_VALUE, 8, 0};

			FILE *f=fopen(INPUT_FILE, "wb");
			fwrite(&s, sizeof(int), 1, f);
			fwrite(v1, sizeof(double), 5, f);
			fwrite(v2, sizeof(double), 5, f);
			fwrite(v3, sizeof(double), 5, f);
			fwrite(v4, sizeof(double), 5, f);
			fwrite(v5, sizeof(double), 5, f);

			fclose(f);
		}






		// each process reads the dimension of the matrix
		FILE *inputFilePtr=fopen(INPUT_FILE, "rb");
		fread(&totalSize, sizeof(int), 1, inputFilePtr);
		printf("Total size=%d\n", totalSize);

		chunkSize=totalSize/numberOfProcesses; // number of columns for each process
		rest=totalSize%numberOfProcesses;

		// each process computes the line it has to start reading from (lines from 0 to K-1)
		int min=(processId<rest)?processId:rest;
		int lineNumber=processId*chunkSize+min;

		// reads chunkSize + 1 columns (namely rows)
		if (processId<rest) chunkSize++;

		// Now processes compute the offset they need for reaching their starting point
		int offset=lineNumber*totalSize*sizeof(double)+sizeof(int);

		printf("Process %d starts from line %d with chunkSize %d and offset %d\n", processId, lineNumber, chunkSize, offset);



		// and sets the inputFilePtr in the correct position
		fseek(inputFilePtr, offset, SEEK_SET);


		// memory allocation
		chunk=malloc(sizeof(double)*chunkSize);
		visited=malloc(sizeof(int)*totalSize);


		// read chunkSize lines from the file
		int i;
		for (i=0; i<chunkSize; i++){
			chunk[i]=malloc(sizeof(double)*totalSize);
			fread(chunk[i], sizeof(double), totalSize, inputFilePtr);
			printf("Process %d read %lf %lf %lf %lf %lf\n", processId, chunk[i][0], chunk[i][1], chunk[i][2], chunk[i][3], chunk[i][4]);
		}


		// NOTE: Process k has nodes with IDs: lineNumber, lineNumber+1, lineNumber+2, ...., lineNumber+chunkSize.

		// insert the initial node among the visited ones
		visited[INITIAL_NODE]=1;


		// Begin of Distributed Prim's Algorithm

		int j, minValNodeIndex=-1, minValEdgeIndex=-1, globalCounter;
		double minVal;

		for (globalCounter=1; globalCounter<totalSize; globalCounter++){
			minVal=MAXIMUM_DOUBLE_VALUE;

			// each node computes the minimum-weight edge among his visited nodes
			for (i=0; i<chunkSize; i++)
				if (visited[i+lineNumber]==1){
					for (j=0; j<totalSize; j++){
						if (chunk[i][j]<minVal && visited[j]!=1){
							minVal=chunk[i][j];
							minValNodeIndex=i;
							minValEdgeIndex=j;
						}
					}
				}

			int minProcessIndex=MASTER;

			// slaves send their minimum value to the master
			if (processId>0)
				MPI_Send(&minVal,1,MPI_DOUBLE, MASTER, 1, MPI_COMM_WORLD);

			// which collects data and chooses the best
			else {
				double receivedVal;
				for (i=1; i<numberOfProcesses; i++){
					MPI_Recv(&receivedVal, 1, MPI_DOUBLE, i, 1, MPI_COMM_WORLD, NULL);
					if (receivedVal<minVal){
						minVal=receivedVal;
						minProcessIndex=i;
					}
				}

			}

			// MASTER tells to slaves who's the winner
			MPI_Bcast(&minProcessIndex, 1, MPI_INT, MASTER, MPI_COMM_WORLD);

			// and that process sends the index of the new node to be inserted among those visited
			MPI_Bcast(&minValEdgeIndex, 1, MPI_INT, minProcessIndex, MPI_COMM_WORLD);

			if (processId==MASTER)
				printf("Iteration %d: added node %d through an edge with weight %lf\n", globalCounter, minValEdgeIndex, minVal);

			visited[minValEdgeIndex]=1;

		}


	MPI_Finalize();

	return 1;
}
