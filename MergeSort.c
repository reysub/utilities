#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#define MASTER 0

void sortData(int*, int);
void fillInputFile(char*);

int MAX_INT = 999;

int main(int argc, char *argv[])
{
	int processId, numberOfProcesses, chunkSize, numberOfElements;;
	int *chunk = NULL;
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &processId);
	MPI_Comm_size(MPI_COMM_WORLD, &numberOfProcesses);


	/********************************************* MASTER PART 1 *********************************************/

	if (processId==MASTER)
	{
		if (argc!=2)
		{
			printf("Error in number of parameters\n");
			MPI_Abort(MPI_COMM_WORLD, 0);
			return 0;
		}

		fillInputFile(argv[1]);


		FILE *inputFilePtr = fopen(argv[1], "rb");

		if (inputFilePtr==NULL)
		{
			printf("Error while opening the input file\n");
			MPI_Abort(MPI_COMM_WORLD, 0);
			return 0;
		}

		fread(&numberOfElements, 1, sizeof(int), inputFilePtr);
		MPI_Bcast(&numberOfElements, 1, MPI_INT, MASTER, MPI_COMM_WORLD);

		int elementsPerProcess = numberOfElements / (numberOfProcesses-1);
		int rest = numberOfElements % (numberOfProcesses-1);

		for (int i = 1; i < numberOfProcesses; ++i)
		{
			chunkSize = elementsPerProcess;
			chunkSize += (i <= rest) ? 1 : 0;

			chunk = realloc(chunk, chunkSize * sizeof(int));

			fread(chunk, chunkSize, sizeof(int), inputFilePtr);

			MPI_Send(&chunkSize, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
			MPI_Send(chunk, chunkSize, MPI_INT, i, 1, MPI_COMM_WORLD);
		}

		fclose(inputFilePtr);


	/********************************************* MASTER PART 2 *********************************************/

		int *receivedMins = malloc(numberOfProcesses * sizeof(int));
		int minVal, minIndex;

		for (int globalIterator = 0; globalIterator < numberOfElements; ++globalIterator)
		{
			minVal=MAX_INT;
			minIndex=0;

			MPI_Gather(&MAX_INT, 1, MPI_INT, receivedMins, 1, MPI_INT, MASTER, MPI_COMM_WORLD);
			for (int i = 0; i < numberOfProcesses; ++i)
			{
				if (receivedMins[i]<minVal)
				{
					minVal = receivedMins[i];
					minIndex = i;
				}
			}

			printf("Num %d = %d\n", globalIterator+1, minVal);
			MPI_Bcast(&minIndex, 1, MPI_INT, MASTER, MPI_COMM_WORLD);
		}

		free(receivedMins);
	}



	/********************************************* SLAVES PART 1 *********************************************/

	else
	{
		MPI_Bcast(&numberOfElements, 1, MPI_INT, MASTER, MPI_COMM_WORLD);
		MPI_Recv(&chunkSize, 1, MPI_INT, MASTER, 0, MPI_COMM_WORLD, NULL);
		chunk = realloc(chunk, chunkSize * sizeof(int));
		MPI_Recv(chunk, chunkSize, MPI_INT, MASTER, 1, MPI_COMM_WORLD, NULL);

		sortData(chunk, chunkSize);


	/********************************************* SLAVES PART 2 *********************************************/

		int position = 0, winner;

		for (int globalIterator = 0; globalIterator < numberOfElements; ++globalIterator)
		{

			if (position < chunkSize)
			{
				MPI_Gather(&chunk[position], 1, MPI_INT, NULL, 1, MPI_INT, MASTER, MPI_COMM_WORLD);
			} else {
				MPI_Gather(&MAX_INT, 1, MPI_INT, NULL, 1, MPI_INT, MASTER, MPI_COMM_WORLD);
			}

			MPI_Bcast(&winner, 1, MPI_INT, MASTER, MPI_COMM_WORLD);

			position += (winner == processId) ? 1 : 0;
		}
	}



	free(chunk);

	MPI_Finalize();
	return 0;
}




void sortData(int *chunk, int size)
{
	int tmp;
	for (int i = 0; i < size; ++i)
	{
		for (int j = 0; j < size; ++j)
		{
			if (chunk[j]>chunk[j+1])
			{
				tmp = chunk[j];
				chunk[j] = chunk[j+1];
				chunk[j+1] = tmp;
			}
		}
	}
}

void fillInputFile(char *path)
{
	int a = 20;
	int b[] = {1,5,9,15,15,1,4,84,3,4,38,48,3,54,8,6,4,4,87,6};

	FILE *f=fopen(path, "wb");
	fwrite(&a, 1, sizeof(int), f);
	fwrite(&b, a, sizeof(int), f);
	fclose(f);
}

