/*
 * Implementation of distributed Prim's Algorithm
 *
 */

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MASTER 0
#define SEPARATOR ","
#define MAX_INT 9999

#define MAX_CHARS_PER_LINE 1024

int main(int argc, char *argv[])
{
	int processId, numberOfProcess, chunkSize, numberOfNodes, startingNode, chosenNode;
	int *chunk = NULL;

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &processId);
	MPI_Comm_size(MPI_COMM_WORLD, &numberOfProcess);

	/*********************************************** MASTER ***********************************************/

	if (processId==MASTER)
	{
		if (argc!=4)
		{
			printf("Error in number of parameters\n");
			MPI_Abort(MPI_COMM_WORLD, 0);
			return 0;
		}

		FILE *inputFilePtr = fopen(argv[1], "r");
		if (inputFilePtr==NULL)
		{
			printf("Error while opening the input file\n");
			MPI_Abort(MPI_COMM_WORLD, 0);
			return 0;
		}

		printf("Starting node = %d\n", atoi(argv[3]));
		chosenNode = atoi(argv[3]) - 1;
		numberOfNodes = atoi(argv[2]);

		MPI_Bcast(&numberOfNodes, 1, MPI_INT, MASTER,MPI_COMM_WORLD);

		int nodesPerProcess = numberOfNodes / numberOfProcess;
		int rest = numberOfNodes % numberOfProcess;

		char *line = malloc(MAX_CHARS_PER_LINE * sizeof(char));

		for (int i = 1; i < numberOfProcess; ++i)
		{
			chunkSize = nodesPerProcess;
			chunkSize += (processId<=rest) ? 1 : 0;

			chunk = realloc(chunk, chunkSize * numberOfNodes * sizeof(int));

			for (int j = 0; j < chunkSize; ++j)
			{
				fgets(line, MAX_CHARS_PER_LINE, inputFilePtr);

				chunk[j * numberOfNodes] = atoi(strtok(line, SEPARATOR));

				for (int k = 1; k < numberOfNodes; ++k)
				{
					chunk[j * numberOfNodes + k] = atoi(strtok(NULL, SEPARATOR));
				}
			}

			startingNode = nodesPerProcess * (i-1);
			startingNode += (i<=rest) ? (i-1) : rest;
			MPI_Send(&startingNode, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
			MPI_Send(&chunkSize, 1, MPI_INT, i, 1, MPI_COMM_WORLD);
			MPI_Send(chunk, chunkSize * numberOfNodes, MPI_INT, i, 2, MPI_COMM_WORLD);
		}

		chunkSize = nodesPerProcess;
		chunk = realloc(chunk, chunkSize * numberOfNodes * sizeof(int));

		for (int j = 0; j < chunkSize; ++j)
		{
			fgets(line, MAX_CHARS_PER_LINE, inputFilePtr);
			chunk[j * numberOfNodes] = atoi(strtok(line, SEPARATOR));

			for (int k = 1; k < numberOfNodes; ++k)
			{
				chunk[j * numberOfNodes + k] = atoi(strtok(NULL, SEPARATOR));
			}
		}

		startingNode = numberOfNodes - nodesPerProcess;

		free(line);
		fclose(inputFilePtr);
	}

	/*********************************************** SLAVES ***********************************************/

	else
	{
		MPI_Bcast(&numberOfNodes, 1, MPI_INT, MASTER,MPI_COMM_WORLD);
		MPI_Recv(&startingNode, 1, MPI_INT, MASTER, 0, MPI_COMM_WORLD, NULL);
		MPI_Recv(&chunkSize, 1, MPI_INT, MASTER, 1, MPI_COMM_WORLD, NULL);
		chunk = realloc(chunk, chunkSize * numberOfNodes * sizeof(int));
		MPI_Recv(chunk, chunkSize * numberOfNodes, MPI_INT, MASTER, 2, MPI_COMM_WORLD, NULL);
	}

	/*********************************************** COMMON WORK ***********************************************/

	int min, minIndex;
	int *visited = malloc(numberOfNodes * sizeof(int));

	for (int globalIterator = 1; globalIterator < numberOfNodes; ++globalIterator)
	{
		MPI_Bcast(&chosenNode, 1, MPI_INT, MASTER, MPI_COMM_WORLD);
		visited[chosenNode]=1;

		min=MAX_INT, minIndex=-1;

		for (int j = 0; j < chunkSize; ++j)
		{
			if (visited[j]==1)
			{
				for (int k = 0; k < numberOfNodes; ++k)
				{
					if (chunk[j * numberOfNodes + k] < min && visited[k]!=1 && chunk[j * numberOfNodes + k] > 0)
					{
						min = chunk[j * numberOfNodes + k];
						minIndex = k + startingNode;
					}
				}
			}
		}

		if (processId!=MASTER)
		{
			MPI_Send(&min, 1, MPI_INT, MASTER, 3, MPI_COMM_WORLD);
			MPI_Send(&minIndex, 1, MPI_INT, MASTER, 4, MPI_COMM_WORLD);
		}

		else
		{
			int recvMin, recvMinIndex;
			for (int i = 1; i < numberOfProcess; ++i)
			{
				MPI_Recv(&recvMin, 1, MPI_INT, i, 3, MPI_COMM_WORLD, NULL);
				MPI_Recv(&recvMinIndex, 1, MPI_INT, i, 4, MPI_COMM_WORLD, NULL);

				if (recvMin < min)
				{
					min=recvMin;
					minIndex=recvMinIndex;
				}
			}
			chosenNode = minIndex;
			printf("added %d through an edge of value %d\n", minIndex+1, min);
		}
	}

	free(chunk); free(visited);

	MPI_Finalize();
	return 0;
}
