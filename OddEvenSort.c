/*
 * Implementation of distributed odd-even sort (Exam 8 September 2010)
 *
 * FILE STRUCTURE:
 * first line= integer representing the number of elements
 * other lines= an integer to be sorted for each line
 *
 * PROCEDURE:
 * Each node has a portion (chunk) of data
 * During an iteration, node k sends his chunk to node k+1
 * node k+1 computes the sorted chunk, splits it in half and sends half of it back to node k
 *
 * ASSUMPTION: a node can handle 2 chunks of data in memory
 *
 */

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#define MASTER 0
#define inputFile "../data/input.bin"
#define outputFile "../data/output.bin"

void sort (int*, int);
void fillInputFile(char*, int);
void printVector(int*, int);

int main (int argc, char** argv){

	// common local variable declaration
	int processID, numberOfProcesses, chunkSize;
	int *chunk;

	// init mpi environment
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &processID);
	MPI_Comm_size(MPI_COMM_WORLD, &numberOfProcesses);

	// MASTER'S WORK
	if (processID==0) {

		// write something in the input file
		fillInputFile(inputFile, 15); printf("\n");

		// master's local variable declaration
		int totalNumberOfElements, result, rest;

		// read the number of elements
		FILE *inputFilePtr=fopen(inputFile, "rb");
		if (inputFilePtr!=NULL){
			fread(&totalNumberOfElements, sizeof(int), 1, inputFilePtr);

			// compute the chunk for each process
			// NOTE: result is the fair distribution, assuming rest = 0
			// while numberOfElements is the number of elements actually sent
			result=totalNumberOfElements/numberOfProcesses;
			rest=totalNumberOfElements%numberOfProcesses;

			int counter;
			for (counter=1; counter<numberOfProcesses; counter++){ // for each slave

				// consider the rest
				if (rest>0)
					chunkSize=result+1;
				else
					chunkSize=result;

				// allocate memory for a chunk
				chunk=(int*)malloc(sizeof(int)*chunkSize);

				// read numberOfElements numbers
				int it;
				for (it=0; it<chunkSize; it++)
					fread((chunk+it), sizeof(int), 1, inputFilePtr);

				// send data to a slave
				MPI_Send(&chunkSize, 1, MPI_INT, counter, 0, MPI_COMM_WORLD);
				MPI_Send(chunk, chunkSize, MPI_INT, counter, 1, MPI_COMM_WORLD);

				// free the memory
				free(chunk);

				// decrease the number of "rest elements"; it'll become negative, not a problem...
				rest--;
			}

			// finally master reads his part too (rest is <=0 for sure)
			chunkSize=result;
			chunk=(int*)malloc(sizeof(int)*chunkSize);
			int it;
			for (it=0; it<chunkSize; it++)
				fread((chunk+it), sizeof(int), 1, inputFilePtr);

			// test print
			printf ("master has the vector: ");
			printVector(chunk, chunkSize);
			printf("\n");

			fclose(inputFilePtr);
		}
		else
			printf ("error while opening the file");
	}

	// BEGIN OF SLAVES' WORK (they just have to receive data from MASTER)
	else {

		// read the size of the chunk
		MPI_Recv(&chunkSize, 1, MPI_INT, MASTER, 0, MPI_COMM_WORLD, NULL);

		// store the chunk
		chunk=(int*)malloc(sizeof(int)*chunkSize);
		MPI_Recv(chunk, chunkSize, MPI_INT, MASTER, 1, MPI_COMM_WORLD, NULL);

		// test prints
		printf ("process %d has received the vector: ", processID);
		printVector(chunk, chunkSize);
		printf("\n");
	}

	// NOW EACH NODE HAS ITS OWN CHUNK.
	// BEGIN OF THE COMMON PARALLEL WORK: DISTRIBUTED ODD-EVEN SORT
	int globalIterator; int maxIterations=numberOfProcesses+1;
	for (globalIterator=2; globalIterator<maxIterations+2; globalIterator++)

		// SENDER PART - if it's a "sender process", and exists a process with rank +1
		if (processID%2==globalIterator%2 && processID<numberOfProcesses-1){

			MPI_Send(&chunkSize, 1, MPI_INT, processID+1, globalIterator, MPI_COMM_WORLD);
			MPI_Send(chunk, chunkSize, MPI_INT, processID+1, globalIterator+maxIterations, MPI_COMM_WORLD);
			printf("process %d has sent to process %d the chunk: ", processID, processID+1);
			printVector(chunk, chunkSize); printf("\n");

			// wait for the result from node +1
			MPI_Recv(chunk, chunkSize, MPI_INT, processID+1, globalIterator+2*maxIterations, MPI_COMM_WORLD, NULL);
			printf("process %d has received from process %d the chunk: ", processID, processID+1);
			printVector(chunk, chunkSize); printf("\n");
		}

	// RECEIVER PART - otherwise (master won't receive data, since does not exist a process with a minor rank)
		else if(processID%2!=globalIterator%2 && processID>MASTER)  {

			// receive data from that process
			int newChunkSize; int* newChunk;
			MPI_Recv(&newChunkSize, 1, MPI_INT, processID-1, globalIterator, MPI_COMM_WORLD, NULL);
			newChunk=(int*)malloc(newChunkSize*sizeof(int));
			MPI_Recv(newChunk, newChunkSize, MPI_INT, processID-1, globalIterator+maxIterations, MPI_COMM_WORLD, NULL);

			printf("process %d has received from process %d the chunk: ", processID, processID-1);
			printVector(newChunk, newChunkSize); printf("\n");

			// put everything together
			int i; int* commonChunk=(int*)malloc((newChunkSize+chunkSize)*sizeof(int));
			for (i=0; i< chunkSize; i++)
				commonChunk[i]=chunk[i];
			for (i=0; i< newChunkSize; i++)
				commonChunk[i+chunkSize]=newChunk[i];

			// sort this common chunk
			sort(commonChunk, newChunkSize+chunkSize);

			for (i=0; i< newChunkSize; i++)
				newChunk[i]=commonChunk[i];
			for (i=newChunkSize; i<newChunkSize+chunkSize; i++)
				chunk[i-newChunkSize]=commonChunk[i];

			// send back part of data
			MPI_Send(newChunk, newChunkSize, MPI_INT, processID-1, globalIterator+2*maxIterations, MPI_COMM_WORLD);
			printf("process %d has sent to process %d the chunk: ", processID, processID-1);
			printVector(newChunk, newChunkSize); printf("\n");

			// free memory
			free(newChunk);
			free(commonChunk);
		}


	// END OF COMPUTATION; NOW NODE 0 HAS THE FIRST SORTED CHUNK, NODE 1 THE SECOND, AND SO ON

	// so each slave sends his chunk to the master
	if (processID>MASTER){
		MPI_Send(&chunkSize, 1, MPI_INT, MASTER, 3*maxIterations+50, MPI_COMM_WORLD);
		MPI_Send(chunk, chunkSize, MPI_INT, MASTER, 3*maxIterations+51, MPI_COMM_WORLD);
	}

	// while the master will write the result in the output file
	else {
		FILE *outputFilePtr=fopen(outputFile, "wb");

		if (outputFilePtr!=NULL){
			int newChunkSize, it, jt; int* newChunk;

			// write master's part first
			printf("master's final data: ");
			printVector(chunk, chunkSize); printf("\n");
			for (jt=0; jt<chunkSize; jt++)
				fwrite(&chunk[jt], 1, sizeof(int), outputFilePtr);

			// for each slave
			for (it=1; it<numberOfProcesses; it++){

				// receive data from it
				MPI_Recv(&newChunkSize, 1, MPI_INT, it, 3*maxIterations+50, MPI_COMM_WORLD, NULL);
				newChunk=(int*)malloc(newChunkSize*sizeof(int));
				MPI_Recv(newChunk, newChunkSize, MPI_INT, it, 3*maxIterations+51, MPI_COMM_WORLD, NULL);

				printf("master has received final data from process %d: ", it);

				// print the result (just for test)
				printVector(newChunk, newChunkSize); printf("\n");

				// write it into the output file
				for (jt=0; jt<newChunkSize; jt++)
					fwrite(&newChunk[jt], 1, sizeof(int), outputFilePtr);
			}

			free (newChunk);
			fclose(outputFilePtr);
		}
		else
			printf ("error while opening the file");
	}

	free(chunk);
	MPI_Finalize();
	return 0;
}


// SIMPLE SORTING FUNCTION (BUBBLE SORT)
void sort (int* vector, int size){
	int i, j, tmp;
	for (i = 0 ; i < size-1; i++)
		for (j = 0 ; j<size-1; j++)
			if (vector[j] > vector[j+1]){
				tmp=vector[j];
				vector[j]=vector[j+1];
				vector[j+1]=tmp;
			}
}

// auxiliary functions, just for testing purpose
// generates n random numbers between 0 and 2n, writes them in the input file and prints them too
void fillInputFile(char* file, int n){
	FILE *fp=fopen(file, "wb");
	if (fp!=NULL){
		int i, r;
		fwrite(&n, sizeof(int), 1, fp);
		printf("vector = [");
		for (i=0; i<n-1; i++){
			r = rand()%(n*2);
			fwrite(&r, sizeof(int), 1, fp);
			printf("%d, ", r);
		}
		r = rand()%(n*2);
		fwrite(&r, sizeof(int), 1, fp);
		printf("%d]", r);
		fclose(fp);
	}
}

// prints a vector
void printVector(int* vector, int n){
	int i;
	printf("[");
	for (i=0; i<n-1; i++)
		printf("%d, ", vector[i]);
	printf("%d]", vector[n-1]);
}
