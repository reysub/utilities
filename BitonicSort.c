/*
 * Implementation of distributed bitonic sort
 *
 * FILE STRUCTURE:
 * first line= integer representing the number of elements
 * other lines= an integer to be sorted for each line
 *
 * PROCEDURE:
 * Each node has a portion (chunk) of data
 * with "computeNumberOfBits" i compute the number of bits necessary for the processes' representation
 * with "int2bin" and "bin2int" i can covert from bin to dec and vice versa
 *
 * During an iteration, node k computes his binary representation and consecutively the process it has to interact with
 * the node with interesting bit=0 sends data, the one with bit=1 receives data,
 * computes the sorted chunk, splits it in half and sends half of it back
 *
 * ASSUMPTION:
 *  - a node can handle 2 chunks of data in memory
 *  - number of processes is 2^d
 *
 *  NOTE:
 *  binary are represented considering char[0] as MSB,
 *  so char p[4]={1, 1, 0, 1} = 13
 *
 */

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define MASTER 0
#define inputFile "../data/input.bin"
#define outputFile "../data/output.bin"

void sort (int*, int);
int computeNumberOfBits(int);
void int2bin(int, int*, int);
int bin2int(int*, int);
void fillInputFile(char*, int);
void fillBitonicInputFile(char*, int);
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
		fillBitonicInputFile(inputFile, 5); printf("\n");

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
	// BEGIN OF THE COMMON PARALLEL WORK: DISTRIBUTED BITONIC SORT
	int globalIterator, numberOfIterations=computeNumberOfBits(numberOfProcesses); // NOTE: number Of Iterations = number of bits
	int binaryId[numberOfIterations];

	if (numberOfIterations!=-1){

		for (globalIterator=0; globalIterator<numberOfIterations; globalIterator++){
			int2bin(processID, binaryId, numberOfIterations);

			// SENDER PART - if it's a "sender process"
			if (binaryId[globalIterator]==0){
				binaryId[globalIterator]=1; // compute which will be the receiver and send it data
				MPI_Send(&chunkSize, 1, MPI_INT, bin2int(binaryId, numberOfIterations), globalIterator+2, MPI_COMM_WORLD);
				MPI_Send(chunk, chunkSize, MPI_INT, bin2int(binaryId, numberOfIterations), globalIterator+2+numberOfIterations, MPI_COMM_WORLD);

				printf("process %d has sent to process %d the chunk: ", processID, bin2int(binaryId, numberOfIterations));
				printVector(chunk, chunkSize); printf("\n");

				// receive sorted data
				MPI_Recv(chunk, chunkSize, MPI_INT, bin2int(binaryId, numberOfIterations), globalIterator+2+2*numberOfIterations, MPI_COMM_WORLD, NULL);
				printf("process %d has received from process %d the chunk: ", processID, bin2int(binaryId, numberOfIterations));
				printVector(chunk, chunkSize); printf("\n");
			}

			// RECEIVER PART - otherwise
			else {

				// receive data from that process
				int *newChunk; int newChunkSize; binaryId[globalIterator]=0;
				MPI_Recv(&newChunkSize, 1, MPI_INT, bin2int(binaryId, numberOfIterations), globalIterator+2, MPI_COMM_WORLD, NULL);
				newChunk=(int*)malloc(sizeof(int)*newChunkSize);
				MPI_Recv(newChunk, newChunkSize, MPI_INT, bin2int(binaryId, numberOfIterations), globalIterator+2+numberOfIterations, MPI_COMM_WORLD, NULL);

				printf("process %d has received from process %d the chunk: ", processID, bin2int(binaryId, numberOfIterations));
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
				MPI_Send(newChunk, newChunkSize, MPI_INT, bin2int(binaryId, numberOfIterations), globalIterator+2+2*numberOfIterations, MPI_COMM_WORLD);

				printf("process %d has sent to process %d the chunk: ", processID, bin2int(binaryId, numberOfIterations));
				printVector(newChunk, newChunkSize); printf("\n");

				// free memory
				free(newChunk);
				free(commonChunk);
			}

		}


		// END OF COMPUTATION; NOW NODE 0 HAS THE FIRST SORTED CHUNK, NODE 1 THE SECOND, AND SO ON

		// so each slave sends his chunk to the master
		if (processID>MASTER){
			MPI_Send(&chunkSize, 1, MPI_INT, MASTER, 3*numberOfIterations+50, MPI_COMM_WORLD);
			MPI_Send(chunk, chunkSize, MPI_INT, MASTER, 3*numberOfIterations+51, MPI_COMM_WORLD);
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
					MPI_Recv(&newChunkSize, 1, MPI_INT, it, 3*numberOfIterations+50, MPI_COMM_WORLD, NULL);
					newChunk=(int*)malloc(newChunkSize*sizeof(int));
					MPI_Recv(newChunk, newChunkSize, MPI_INT, it, 3*numberOfIterations+51, MPI_COMM_WORLD, NULL);

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
				printf ("error while opening the file\n");
		}

		free(chunk);

	} else if(processID==MASTER) printf("********** TERMINATION. Number of processes isn't a power of 2 **********\n");

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

// COMPUTES THE NUMBER OF BITS NEEDED FOR REPRESENTING AN INT
int computeNumberOfBits(int procNum){
	int k=0;
	while (((procNum % 2) == 0) && procNum > 1){
		procNum /= 2;
		k++;
	}
	if (procNum == 1)
		return k;
	else
		return -1;
}

// COMPUTES THE BINARY REPRESENTATION OF AN INTEGER, USING EXACTLY BITSNUMBER BITS
void int2bin(int dec, int *bin, int bitsNumber){
	int i, tmp[bitsNumber];
	for (i=0; i<bitsNumber; i++)
		tmp[i]=0;
	i=0;
	while(dec>=1){
		tmp[i]=dec%2;
		dec/=2;
		i++;
	}
	for (i=0; i<bitsNumber; i++)
		bin[i]=tmp[bitsNumber-i-1];
}

// CONVERSION FROM A BINARY TO A DECIMAL INT
int bin2int(int* bin, int size){
	int i, res=0;
	for (i=0; i<size; i++)
		res+=bin[i]*pow(2,size-i-1);
	return res;
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

// generates 2n random numbers between 0 and 4n in a bitonic order, writes them in the input file and prints them too
void fillBitonicInputFile(char* file, int n){
	FILE *fp=fopen(file, "wb");
	if (fp!=NULL){
		int i; int r[n];
		n*=2;
		fwrite(&n, sizeof(int), 1, fp);

		for (i=0; i<n/2; i++)
			r[i] = rand()%(n*2);

		sort(r, n/2);
		printf("vector = [");


		for (i=0; i<n/2; i++){
			fwrite(&r[i], sizeof(int), 1, fp);
			printf("%d, ", r[i]);
			r[i] = rand()%(n*2);
		}

		sort(r, n/2);

		for (i=n/2-1; i>0; i--){
			fwrite(&r[i], sizeof(int), 1, fp);
			printf("%d, ", r[i]);
		}

		fwrite(&r[0], sizeof(int), 1, fp);
		printf("%d]", r[0]);

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
