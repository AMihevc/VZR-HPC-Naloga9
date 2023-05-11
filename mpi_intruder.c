// module load OpenMPI/4.1.0-GCC-10.2.0 
// mpicc mpi_nb.c -o mpi_nb
// srun --reservation=fri --mpi=pmix -n2 -N2 ./mpi_nb

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <mpi.h>
#include <time.h>
#include <math.h>

#define SIZE 100000000

int main(int argc, char *argv[]) 
{ 
	int rank; // process rank 
	int num_procs; // total number of processes 
	int source; // sender rank
	int destination; // receiver rank 
	int tag = 0; // message tag 
	int buffer[1]; // message buffer 
	MPI_Status status; // message status
	MPI_Request request; // MPI request
	int found_flag; // request status flag
	char node_name[MPI_MAX_PROCESSOR_NAME]; //node name
	int name_len; //true length of node name
    int *scaned_indexes; //array of indexes scaned by each process
    
	MPI_Init(&argc, &argv); // initialize MPI 
	
	MPI_Comm_rank(MPI_COMM_WORLD, &rank); // get process rank 
	MPI_Comm_size(MPI_COMM_WORLD, &num_procs); // get number of processes
	MPI_Get_processor_name( node_name, &name_len ); // get node name

    //initialize the array
    char *array = NULL;

    // set the seed for random number generator
    int seed = time(NULL) + rank;
    // if (rank == 0) printf("[DEBUG]Seed for process %d is %d\n", rank, seed);
    srand(seed);

    if (rank == 0) {
        // create an array of size 100M bytes filed with 1s and 0s
        array = (char *)malloc(SIZE*sizeof(char));
        
        for (int i = 0; i < SIZE; i++) {
            array[i] = rand() % 2;
        }

        // place the intruder (2) at the start of the array
        int place2 = 2;

        //at the end of the array
        //place2 = SIZE - 2;

        //at the middle of the array
        place2 = SIZE/2 + 2 ;

        //in the middle of the second half of the array
        //place2 = SIZE/2 + SIZE/4 + 14;

        //randomly 
        // place2 = rand() % SIZE;

        //printf("[DEBUG]Intruder placed at %d\n", place2);

        array[place2] = 2;

        // inicialize the scaned_indexes array
        scaned_indexes = (int *)malloc(num_procs*sizeof(int));
    }

    //start the timer
    double start_time = MPI_Wtime();

    // calculate the size of the array for each process
    
    // set up the count and displacement arrays
    int *counts = (int *)malloc(num_procs*sizeof(int));
    int *displs = (int *)malloc(num_procs*sizeof(int));

    int my_size = ceil(SIZE/num_procs); //TODO calculate the size of the array for each process if the array is not divisible by the number of processes

    // inicialize the count and displacement arrays
    for (int i = 0; i < num_procs; i++) {
        counts[i] = my_size;
        displs[i] = i*my_size;
    }

    // incialize the recive array for each process with zeros
    char *my_recv_array = calloc(my_size, sizeof(char));

    // scatter the array to all processes
    MPI_Scatterv(array, counts, displs, MPI_CHAR, my_recv_array, my_size, MPI_CHAR, 0, MPI_COMM_WORLD);
    
    //start Irecv to buffer
    //buffer = (int *)calloc(1, sizeof(int));
	MPI_Irecv(buffer, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &request);

	//Initialize request flag and check if Irecv already completed
	MPI_Test(&request, &found_flag, &status);
    
    int index = 0;
	found_flag = 0;
    int i_found_two = 0;
    while (index < my_size)
    {	
        
		if(found_flag){
            //some other process found the intruder
            break;
        }
        else if (my_recv_array[index] == 2){
            //I found the intruder
			//Notify all other processes
			printf("Process %d found intruder on index %d \n",rank, displs[rank]+index);
			fflush(stdout);
			for(int p=0; p<num_procs; p++){
				if (p!=rank)
					MPI_Send(&rank, 1, MPI_INT, p, rank, MPI_COMM_WORLD);
			}
            i_found_two = 1;
			break;
		}
		// Check if Irecv completed 
        if( index % 10000 == 0) MPI_Test(&request, &found_flag, &status);
        
        //increase the counter
        index++;
    }

    //if I found the intruder cancel Irecv
	if (i_found_two){ 
		MPI_Cancel(&request);
		MPI_Wait(&request, &status); //Wait for cancel to complete
	}

	//Communicate the amount of work by each process

	MPI_Gather(&index, 1, MPI_INT, scaned_indexes, 1, MPI_INT, 0, MPI_COMM_WORLD);
	if (rank==0){
        //print the amount of work by each process
        for (int p=0;p<num_procs;p++){
            printf("Work by process %d: %d\n",p, scaned_indexes[p]);
        }

        //print the time taken
        printf("Time taken: %f\n", MPI_Wtime()-start_time);
    }

    //if I did not find the intruder and came through the whole array cancel Irecv
    if(index == my_size-1){
        //I did not find the intruder
        //printf("Process %d did not find the intruder\n", rank);
        MPI_Cancel(&request);
        MPI_Wait(&request, &status); //Wait for cancel to complete
    }
		
	MPI_Finalize();

    //clean up
    free(array);
    free(counts);
    free(displs);
    
    if (rank == 0)
    {
        free(scaned_indexes);
    }
	return 0; 
}
/* RESULTS: 

Time for 1 proc:
    START OF ARRAY: 
        Process 0 found intruder on index 2 
        Work by process 0: 2
        Time taken: 0.045906

    END OF ARRAY: 
        Process 0 found intruder on index 99999998 
        Work by process 0: 99999998
        Time taken: 0.730987

    MIDDLE OF ARRAY:
        Process 0 found intruder on index 50000002 
        Work by process 0: 50000002
        Time taken: 0.390466

Time for 4 procs:
1 NODE: 
    START OF ARRAY: 
        Process 0 found intruder on index 2 
        Work by process 0: 2
        Work by process 1: 5040001
        Work by process 2: 2530001
        Work by process 3: 10001
        Time taken: 0.064175
    
    END OF ARRAY:
        Process 3 found intruder on index 99999998 
        Work by process 0: 24580001
        Work by process 1: 25000000
        Work by process 2: 25000000
        Work by process 3: 24999998
        Time taken: 0.237467

    MIDDLE OF ARRAY:
        Process 2 found intruder on index 50000002 
        Work by process 0: 1
        Work by process 1: 2370001
        Work by process 2: 2
        Work by process 3: 1
        Time taken: 0.068171


2 NODES: 
    START OF ARRAY:
        Process 0 found intruder on index 2 
        Work by process 0: 2
        Work by process 1: 6680001
        Work by process 2: 4080001
        Work by process 3: 30001
        Time taken: 0.079292
    
    END OF ARRAY:
        Process 3 found intruder on index 99999998
        Work by process 0: 21930001
        Work by process 1: 25000000
        Work by process 2: 25000000
        Work by process 3: 24999998
        Time taken: 0.236102

    MIDDLE OF ARRAY:
        Work by process 0: 1
        Work by process 1: 3490001
        Work by process 2: 2
        Work by process 3: 1
        Time taken: 0.079114

Time for 16 procs:
1 NODE: 
    START OF ARRAY: 
        Process 0 found intruder on index 2
        Work by process 0: 2
        Work by process 1: 6250000
        Work by process 2: 6250000
        Work by process 3: 6250000
        Work by process 4: 6250000
        Work by process 5: 6250000
        Work by process 6: 6250000
        Work by process 7: 6050001
        Work by process 8: 5030001
        Work by process 9: 4300001
        Work by process 10: 3620001
        Work by process 11: 2890001
        Work by process 12: 2140001
        Work by process 13: 1400001
        Work by process 14: 710001
        Work by process 15: 20001
        Time taken: 0.090290

    END OF ARRAY:
        Process 15 found intruder on index 99999998
        Work by process 0: 6250000
        Work by process 1: 6250000
        Work by process 2: 6250000
        Work by process 3: 6250000
        Work by process 4: 6250000
        Work by process 5: 6250000
        Work by process 6: 6250000
        Work by process 7: 6250000
        Work by process 8: 6250000
        Work by process 9: 6250000
        Work by process 10: 6250000
        Work by process 11: 6250000
        Work by process 12: 6250000
        Work by process 13: 6250000
        Work by process 14: 6250000
        Work by process 15: 6249998
        Time taken: 0.137744
    
    MIDDLE OF ARRAY:
        Process 8 found intruder on index 50000002 
        Work by process 0: 1
        Work by process 1: 5060001
        Work by process 2: 4330001
        Work by process 3: 3610001
        Work by process 4: 2920001
        Work by process 5: 2120001
        Work by process 6: 1420001
        Work by process 7: 690001
        Work by process 8: 2
        Work by process 9: 1
        Work by process 10: 1
        Work by process 11: 1
        Work by process 12: 1
        Work by process 13: 1
        Work by process 14: 1
        Work by process 15: 1
        Time taken: 0.092598

2 NODES 
    START OF ARRAY:
        Process 0 found intruder on index 2 
        Work by process 0: 2
        Work by process 1: 6250000
        Work by process 2: 6250000
        Work by process 3: 6250000
        Work by process 4: 6250000
        Work by process 5: 6250000
        Work by process 6: 6250000
        Work by process 7: 6250000
        Work by process 8: 6250000
        Work by process 9: 6250000
        Work by process 10: 6250000
        Work by process 11: 6250000
        Work by process 12: 6250000
        Work by process 13: 6250000
        Work by process 14: 1220001
        Work by process 15: 30001
        Time taken: 0.159334
    
    END OF ARRAY:
        Process 15 found intruder on index 99999998 
        Work by process 0: 5530001
        Work by process 1: 6250000
        Work by process 2: 6250000
        Work by process 3: 6250000
        Work by process 4: 6250000
        Work by process 5: 6250000
        Work by process 6: 6250000
        Work by process 7: 6250000
        Work by process 8: 6250000
        Work by process 9: 6250000
        Work by process 10: 6250000
        Work by process 11: 6250000
        Work by process 12: 6250000
        Work by process 13: 6250000
        Work by process 14: 6250000
        Work by process 15: 6249998
        Time taken: 0.233092
    
    MIDDLE OF ARRAY:
        Process 8 found intruder on index 50000002 
        Work by process 0: 1
        Work by process 1: 5630001
        Work by process 2: 6250000
        Work by process 3: 6250000
        Work by process 4: 6250000
        Work by process 5: 6250000
        Work by process 6: 6250000
        Work by process 7: 6250000
        Work by process 8: 2
        Work by process 9: 5660001
        Work by process 10: 4650001
        Work by process 11: 3660001
        Work by process 12: 2560001
        Work by process 13: 1390001
        Work by process 14: 500001
        Work by process 15: 1
        Time taken: 0.106627


Time for 32 procs:
1 NODE: 
    START OF ARRAY:
        Process 0 found intruder on index 2
        Work by process 0: 2
        Work by process 1: 3125000
        Work by process 2: 3125000
        Work by process 3: 3125000
        Work by process 4: 3125000
        Work by process 5: 3125000
        Work by process 6: 3125000
        Work by process 7: 3125000
        Work by process 8: 3125000
        Work by process 9: 3125000
        Work by process 10: 3125000
        Work by process 11: 3125000
        Work by process 12: 3125000
        Work by process 13: 3125000
        Work by process 14: 3125000
        Work by process 15: 3125000
        Work by process 16: 3125000
        Work by process 17: 3125000
        Work by process 18: 3125000
        Work by process 19: 3125000
        Work by process 20: 3125000
        Work by process 21: 3125000
        Work by process 22: 3125000
        Work by process 23: 3125000
        Work by process 24: 3125000
        Work by process 25: 3125000
        Work by process 26: 2790001
        Work by process 27: 2270001
        Work by process 28: 1700001
        Work by process 29: 1170001
        Work by process 30: 610001
        Work by process 31: 20001
        Time taken: 0.128842

    END OF ARRAY:
        Process 31 found intruder on index 99999998 
        Work by process 0: 3125000
        Work by process 1: 3125000
        Work by process 2: 3125000
        Work by process 3: 3125000
        Work by process 4: 3125000
        Work by process 5: 3125000
        Work by process 6: 3125000
        Work by process 7: 3125000
        Work by process 8: 3125000
        Work by process 9: 3125000
        Work by process 10: 3125000
        Work by process 11: 3125000
        Work by process 12: 3125000
        Work by process 13: 3125000
        Work by process 14: 3125000
        Work by process 15: 3125000
        Work by process 16: 3125000
        Work by process 17: 3125000
        Work by process 18: 3125000
        Work by process 19: 3125000
        Work by process 20: 3125000
        Work by process 21: 3125000
        Work by process 22: 3125000
        Work by process 23: 3125000
        Work by process 24: 3125000
        Work by process 25: 3125000
        Work by process 26: 3125000
        Work by process 27: 3125000
        Work by process 28: 3125000
        Work by process 29: 3125000
        Work by process 30: 3125000
        Work by process 31: 3124998
        Time taken: 0.149109

    MID OF ARRAY:
        Work by process 0: 1
        Work by process 1: 3125000
        Work by process 2: 3125000
        Work by process 3: 3125000
        Work by process 4: 3125000
        Work by process 5: 3125000
        Work by process 6: 3125000
        Work by process 7: 3125000
        Work by process 8: 3125000
        Work by process 9: 3125000
        Work by process 10: 3100001
        Work by process 11: 2720001
        Work by process 12: 2180001
        Work by process 13: 1680001
        Work by process 14: 1080001
        Work by process 15: 540001
        Work by process 16: 2
        Work by process 17: 1
        Work by process 18: 1
        Work by process 19: 1
        Work by process 20: 1
        Work by process 21: 1
        Work by process 22: 1
        Work by process 23: 1
        Work by process 24: 1
        Work by process 25: 1
        Work by process 26: 1
        Work by process 27: 1
        Work by process 28: 1
        Work by process 29: 1
        Work by process 30: 1
        Work by process 31: 1
        Time taken: 0.131668


2 NODES:

    START OF ARRAY: 
        Process 0 found intruder on index 2 
        Work by process 0: 2
        Work by process 1: 3125000
        Work by process 2: 3125000
        Work by process 3: 3125000
        Work by process 4: 3125000
        Work by process 5: 3125000
        Work by process 6: 3125000
        Work by process 7: 3125000
        Work by process 8: 3125000
        Work by process 9: 3125000
        Work by process 10: 3125000
        Work by process 11: 3125000
        Work by process 12: 3125000
        Work by process 13: 3125000
        Work by process 14: 3125000
        Work by process 15: 3125000
        Work by process 16: 3125000
        Work by process 17: 3125000
        Work by process 18: 3125000
        Work by process 19: 3125000
        Work by process 20: 3125000
        Work by process 21: 3125000
        Work by process 22: 3125000
        Work by process 23: 3125000
        Work by process 24: 3125000
        Work by process 25: 3125000
        Work by process 26: 3125000
        Work by process 27: 2930001
        Work by process 28: 2160001
        Work by process 29: 1740001
        Work by process 30: 760001
        Work by process 31: 30001
        Time taken: 0.256958

    END OF ARRAY:
        Process 31 found intruder on index 99999998 
        Work by process 0: 2820001
        Work by process 1: 3125000
        Work by process 2: 3125000
        Work by process 3: 3125000
        Work by process 4: 3125000
        Work by process 5: 3125000
        Work by process 6: 3125000
        Work by process 7: 3125000
        Work by process 8: 3125000
        Work by process 9: 3125000
        Work by process 10: 3125000
        Work by process 11: 3125000
        Work by process 12: 3125000
        Work by process 13: 3125000
        Work by process 14: 3125000
        Work by process 15: 3125000
        Work by process 16: 3125000
        Work by process 17: 3125000
        Work by process 18: 3125000
        Work by process 19: 3125000
        Work by process 20: 3125000
        Work by process 21: 3125000
        Work by process 22: 3125000
        Work by process 23: 3125000
        Work by process 24: 3125000
        Work by process 25: 3125000
        Work by process 26: 3125000
        Work by process 27: 3125000
        Work by process 28: 3125000
        Work by process 29: 3125000
        Work by process 30: 3125000
        Work by process 31: 3124998
        Time taken: 0.263562

    MID OF ARRAY:
        Process 16 found intruder on index 50000002 
        Work by process 0: 1
        Work by process 1: 3125000
        Work by process 2: 3125000
        Work by process 3: 3125000
        Work by process 4: 3125000
        Work by process 5: 3125000
        Work by process 6: 3125000
        Work by process 7: 3125000
        Work by process 8: 3125000
        Work by process 9: 3125000
        Work by process 10: 3125000
        Work by process 11: 3125000
        Work by process 12: 3125000
        Work by process 13: 3125000
        Work by process 14: 3125000
        Work by process 15: 3125000
        Work by process 16: 2
        Work by process 17: 1
        Work by process 18: 1
        Work by process 19: 1
        Work by process 20: 1
        Work by process 21: 1
        Work by process 22: 1
        Work by process 23: 1
        Work by process 24: 1
        Work by process 25: 1
        Work by process 26: 1
        Work by process 27: 1
        Work by process 28: 1
        Work by process 29: 1
        Work by process 30: 1
        Work by process 31: 1
        Time taken: 0.200539

*/  
