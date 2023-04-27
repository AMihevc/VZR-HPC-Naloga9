// module load OpenMPI/4.1.0-GCC-10.2.0 
// mpicc mpi_nb.c -o mpi_nb
// srun --reservation=fri --mpi=pmix -n2 -N2 ./mpi_nb

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <mpi.h>

int main(int argc, char *argv[]) 
{ 
	int rank; // process rank 
	int num_p; // total number of processes 
	int source; // sender rank
	int destination; // receiver rank 
	int tag = 0; // message tag 
	int buffer[1]; // message buffer 
	MPI_Status status; // message status
	MPI_Request request; // MPI request
	int flag; // request status flag
	char node_name[MPI_MAX_PROCESSOR_NAME]; //node name
	int name_len; //true length of node name
    
	MPI_Init(&argc, &argv); // initialize MPI 
	
	MPI_Comm_rank(MPI_COMM_WORLD, &rank); // get process rank 
	MPI_Comm_size(MPI_COMM_WORLD, &num_p); // get number of processes
	MPI_Get_processor_name( node_name, &name_len ); // get node name
	
	MPI_Irecv(buffer, 1, MPI_INT, MPI_ANY_SOURCE, tag, MPI_COMM_WORLD, &request);
	// Choose a random process to be the sender
	int sender=rand()%num_p;
	//Initialize request flag and check if Irecv already completed
	MPI_Test(&request, &flag, &status);
    int counter = 0;
	while (!flag)
    {	//------------
		//Do some work
		counter++;
		//------------
		if (sender==rank){
			//Notify all other processes
			printf("Process %d is sending from %s!\n",rank, node_name);
			fflush(stdout);
			for(int p=0; p<num_p; p++){
				if (p!=sender)
					MPI_Send(&rank, 1, MPI_INT, p, 0, MPI_COMM_WORLD);
			}
			break;
		}
		// Check if Irecv completed
        MPI_Test(&request, &flag, &status);
    }


	if (sender==rank){ // Cancel Irecv if sender
		MPI_Cancel(&request);
		MPI_Wait(&request, &status); //Wait for cancel to complete
	} else {
		printf("Process %d on %s got message from %d!\n", rank, node_name, buffer[0]);
		fflush(stdout);
	}
	//Communicate the amount of work by each process
	int * counters = (int *)malloc(num_p*sizeof(int));
	MPI_Gather(&counter, 1, MPI_INT, counters, 1, MPI_INT, 0, MPI_COMM_WORLD);
	if (rank==0)
		for (int p=0;p<num_p;p++)
			printf("Work by process %d: %d\n",p, counters[p]);
	MPI_Finalize(); 

	return 0; 
} 
