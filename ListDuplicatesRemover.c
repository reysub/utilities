/*
 *
 * RIMOZIONE DUPLICATI DA UNA LISTA
 *
 *	- File system non condiviso
 *  - Il master lavora
 *
 */

 #include <mpi.h>
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>

 #define MASTER 0

 void fillInputFile(char*);

int main(int argc, char *argv[])
{

 	int processId, numberOfProcesses, chunkSize;
 	int *chunk = NULL;
 	MPI_Init(&argc, &argv);
 	MPI_Comm_rank(MPI_COMM_WORLD, &processId);
 	MPI_Comm_size(MPI_COMM_WORLD, &numberOfProcesses);


 /************************************************* MASTER'S WORK *************************************************/

 	if (processId == MASTER)
 	{

 		// check input parameters
 		if (argc!=2)
 		{
 			printf("Error in number of parameters\n");
 			MPI_Abort(MPI_COMM_WORLD, 0);
 			return 0;
 		}

 		// fill the input file
 		fillInputFile(argv[1]);


 		// open the input file
 		FILE *inputFilePtr = fopen(argv[1], "rb");

 		if (inputFilePtr==NULL)
 		{
 			printf("Error while opening the input file\n");
 			MPI_Abort(MPI_COMM_WORLD, 0);
 			return 0;
 		}

 		// read the number of elements
 		int numberOfElements;
 		fread(&numberOfElements, 1, sizeof(int), inputFilePtr);

 		// compute the amount of work for each process
 		int elementsPerProcess = numberOfElements / numberOfProcesses;
 		int rest = numberOfElements % numberOfProcesses;

 		// for each process
 		for (int i = 1; i < numberOfProcesses; ++i)
 		{
 			// consider the rest
 			chunkSize = elementsPerProcess;
 			chunkSize += (i<rest) ? 1 : 0;

 			// allocate memory
 			chunk = realloc(chunk, chunkSize * sizeof(int));

 			// read data
 			fread(chunk, chunkSize, sizeof(int), inputFilePtr);

 			// and send it
 			MPI_Send(&chunkSize, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
 			MPI_Send(chunk, chunkSize, MPI_INT, i, 1, MPI_COMM_WORLD);
 		}

 		// finally master reads its part
 		chunkSize = elementsPerProcess;
 		chunk = realloc(chunk, chunkSize * sizeof(int));
 		fread(chunk, chunkSize, sizeof(int), inputFilePtr);

 		// close the file
 		fclose(inputFilePtr);
 	}

 /************************************************* SLAVES' WORK *************************************************/
 	else
 	{
 		// receive data
 		MPI_Recv(&chunkSize, 1, MPI_INT, MASTER, 0, MPI_COMM_WORLD, NULL);
 		chunk = malloc(chunkSize * sizeof(int));
 		MPI_Recv(chunk, chunkSize, MPI_INT, MASTER, 1, MPI_COMM_WORLD, NULL);
	}


 /************************************************* COMMON WORK *************************************************/

	// initialization
	int receivedChunkSize; int *receivedChunk=NULL; int *receivedSurvivors=NULL;
	int *survivors=malloc(chunkSize * sizeof(int));

	for (int i = 0; i < chunkSize; ++i)
	{
		survivors[i]=1;
	}


	// each process removes duplicates from its own list first
	for (int i = 0; i < chunkSize; ++i)
	{
		for (int j = 0; j < chunkSize; ++j)
		{
			if ((chunk[i]==chunk[j]) && (i!=j) && (survivors[j]==1))
			{
				survivors[i]=0;
			}
		}
	}

	// then...
 	for (int i = 0; i < numberOfProcesses; ++i)
 	{
 		// if the root
 		if (processId==i)
 		{
 			// just send data
 			MPI_Bcast(&chunkSize, 1, MPI_INT, i, MPI_COMM_WORLD);
 			MPI_Bcast(chunk, chunkSize, MPI_INT, i, MPI_COMM_WORLD);
 			MPI_Bcast(survivors, chunkSize, MPI_INT, i, MPI_COMM_WORLD);
 		}

 		// if another process
 		else
 		{
 			// receive data
 			MPI_Bcast(&receivedChunkSize, 1, MPI_INT, i, MPI_COMM_WORLD);
 			receivedChunk = realloc(receivedChunk, receivedChunkSize * sizeof(int));
 			receivedSurvivors = realloc(receivedSurvivors, receivedChunkSize * sizeof(int));
 			MPI_Bcast(receivedChunk, receivedChunkSize, MPI_INT, i, MPI_COMM_WORLD);
 			MPI_Bcast(receivedSurvivors, receivedChunkSize, MPI_INT, i, MPI_COMM_WORLD);

 			// for each received element
 			for (int i = 0; i < receivedChunkSize; ++i)
 			{
 				// for each element of mine
 				for (int j = 0; j < chunkSize; ++j)
 				{
 					// if i've an equal element not removed yet
 					if (receivedChunk[i]==chunk[j] && receivedSurvivors[i]==1)
 					{
 						// update the survivors
 						survivors[j]=0;
 					}
 				}
 			}
 		}
 	}

 	 free(receivedChunk); free(receivedSurvivors);

 /************************************************* PRINT *************************************************/

 	if (processId==MASTER)
 	{
 		for (int j = 0; j < chunkSize; ++j)
 		{
 			if (survivors[j]==1)
 			{
 				printf("%d\n", chunk[j]);
 			}
 		}

 		for (int i = 1; i < numberOfProcesses; ++i)
 		{
 			MPI_Recv(&chunkSize, 1, MPI_INT, i, 2, MPI_COMM_WORLD, NULL);
 			chunk = malloc(chunkSize * sizeof(int));
 			MPI_Recv(chunk, chunkSize, MPI_INT, i, 3, MPI_COMM_WORLD, NULL);
 			MPI_Recv(survivors, chunkSize, MPI_INT, i, 4, MPI_COMM_WORLD, NULL);

 			for (int j = 0; j < chunkSize; ++j)
 			{
 				if (survivors[j]==1)
 				{
 					printf("%d\n", chunk[j]);
 				}
 			}
 		}
 	}

 	else
 	{
 		MPI_Send(&chunkSize, 1, MPI_INT, MASTER, 2, MPI_COMM_WORLD);
 		MPI_Send(chunk, chunkSize, MPI_INT, MASTER, 3, MPI_COMM_WORLD);
 		MPI_Send(survivors, chunkSize, MPI_INT, MASTER, 4, MPI_COMM_WORLD);
 	}

 	// free memory
 	free(chunk); free(survivors);


	MPI_Finalize();
	return 0;
}




void fillInputFile(char *path)
{
	int a=10;
	int b[]={1,2,1,3,2, 1,2,3,1,3};
	FILE *f = fopen(path, "wb");
	fwrite(&a, 1, sizeof(int), f);
	fwrite(b, a, sizeof(int), f);
	fclose(f);
}
