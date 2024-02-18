/*

Authors: Meghan Grayson and Vaishnavi Karaguppi
Date: February 11, 2024

This program computes a prefix sum with Hillis and Steele's parallel algorithm.
The specified number of processes are created by the parent process. Each process performs the algorithm on a subset of the
input array's elements. Two arrays of size N are used for all intermediate prefix sum array. A barrier is implemented with a
simple counter, so the barrier is implemented with space complexity of O(1).

*/

#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <sys/stat.h>
#include <errno.h>

using namespace std;

/* 
Handle errors and bad input
param      String to be printed to user
*/
void errmsg(string msg) {
    perror(msg.c_str());
    exit(1);
}

/* 
Determines if input values for N and M are valid, and if enough arguments were provided.
param       argCount -- number of total arguments from main
            args -- arguments array from main
return      -1 if invalid, 0 if valie
*/
int verifyArgs(int argCount, char* args[]) {
    int numArgs = 5; // values N, M, input file string, output file string

    /* Clean exit if not enough arguments */
    if(argCount < numArgs) {
        return -1;
    }

    // Temporarily store the first two arguments
    string N = args[1];
    string M = args[2];

    // Verify that N and M are non-negative integers
    for(int i = 0; i < (int)N.length(); i++) {
        if(!isdigit(N[i])) {
            return -1;
        }
    }
    for(int i = 0; i < (int)M.length(); i++) {
        if(!isdigit(M[i])) {
            return -1;
        }
    }

    // Verify values of N and M are greater than 0
    int valN = atoi(args[1]);
    int valM = atoi(args[2]);
    if(valN <= 0 || valM <=0) {
        return -1;
    }

    return 0; // Validated the arguments
}

/* 
Create the input array from the given input file
param       filename -- path of the file to be read
            array -- Pointer to the first element of the input array to be initialized
            N -- the number of values to be read
return      the number of values written, or -1 if unable to open the file
*/
int makeInputArray(string filename, int* array, int N) {
    ifstream in;
    in.open(filename.c_str());

    /* Clean exit if unable to open the file */
    if(!in.is_open()) {
        return -1;
    }

    // Create the array
    int count = 0;
    int currentValue;
    while(in >> currentValue) {
        if(count >= N) { break; } // Do not go out of range
        array[count] = currentValue;
        count++;
    }

    in.close(); // Close the input file
    return count;
}

/* 
Create the output file from the given array
param       filename -- path of the file to be written to
            array -- Pointer to the first element of the output array to be read
            N -- the size of the array
return      0 if successful, or -1 if unable to open the file
*/
int writeOutputArray(string filename, int* array, int N) {
    ofstream out;
    out.open(filename.c_str());

    /* Clean exit if unable to open the file */
    if(!out.is_open()) {
        return -1;
    }

    // Write the array to the output file
    for(int i = 0 ; i < N ; i++) {
        out << array[i] << '\n';
    }
    out.close();

    return 0;
}

/* 
Perform the Hillis and Steele algorithm, executed by child processes
param       thisArray -- Array for the current iteration
            nextArray -- array for the next iteration
            thisProcess -- the number associated with the current process
            processes -- the total number of processes
            iter -- the number of the current iteration
            blockSize -- the number of elements to be handled by the current process
            arraySize -- the size of the arrays
*/
void parallelScan(int* thisArray, int* nextArray, int thisProcess, int processes, int iter, int blockSize, int arraySize) {
    // Determine the range for the current process
    int blockStart = thisProcess * blockSize;
    int blockEnd = blockStart + blockSize;

    // If current process is the last one, blockEnd should be the last element
    if(thisProcess == (processes -1)) { blockEnd = arraySize; }

    int temp = (int)pow(2, iter); // value to be used in the algorithm

    // Perform algorithm
    for(int k = blockStart ; k < blockEnd ; k++) {
        if(k >= arraySize) { break; } // Out of range, don't calculate
        if(k < temp) {
            nextArray[k] = thisArray[k]; // Same value copied for next iteration
        }
        else {
            nextArray[k] = thisArray[k] + thisArray[k - temp];
        }
    }
}

/* 
Synchronize the processes with the barrier
param       turn -- Pointer to the barrier counter
            thisProcess -- the number associated with the current process
            iter -- the number of the current iteration
            processes -- the total number of processes
*/
void synchronize(int* turn, int thisProcess, int iter, int processes) {
    while(turn[0] != ((iter*processes) + thisProcess)); // Current process must wait for its turn
    turn[0]++; // Increment the barrier since it's our turn
    while(turn[0] < ((iter+1)*processes)); // Wait for all processes to have their turn for this iteration
}

/*
Swap the array pointers.
param       arrayOne -- a pointer to the first element of the first array
            arrayTwo -- a pointer to the first element of the second arrray
*/
void swapArrays(int* arrayOne, int* arrayTwo) {
    int* temp = arrayOne;
    arrayOne - arrayTwo;
    arrayTwo = temp;
}

/*
Remove the shared memory segments associated with the given memory IDs
param       memOne, memTwo, memThree -- Memory IDs of the shared memory segments
            ptrOne, ptrTwo, ptrThree -- Pointers attached to the shared memory segments
*/
void removeMemory(int memOne, int memTwo, int memThree, int* ptrOne, int* ptrTwo, int* ptrThree) {
    // Detach from shared memory
    shmdt((void*) ptrOne);
    shmdt((void*) ptrTwo);
    shmdt((void*) ptrThree);
    // Remove the shared memory segment
    shmctl(memOne, IPC_RMID, NULL);
    shmctl(memTwo, IPC_RMID, NULL);
    shmctl(memThree, IPC_RMID, NULL);
}


/* Start of the main function */
int main(int argc, char* argv[]) {
    // Check the number of arguments and values for N and M are valid
    if(verifyArgs(argc, argv) < 0) {
        errmsg("Invalid arguments provided.\n"); // clean exit
    }

    /* Assign the input args */
    int arrSize = atoi(argv[1]);
    int numProcesses = atoi(argv[2]);
    string infileName = argv[3];
    string outfileName = argv[4];

    /* If there are more cores than N, no need to make additional processes */
    if(numProcesses > arrSize) { numProcesses = arrSize; }

    /* Create the shared memory segments */
    size_t memSize1 = sizeof(int)*arrSize;
    size_t memSize2 = sizeof(int)*arrSize;
    size_t memSize3 = sizeof(int);
    int memID1 = shmget(IPC_PRIVATE, memSize1, S_IRUSR | S_IWUSR );
    int memID2 = shmget(IPC_PRIVATE, memSize2, S_IRUSR | S_IWUSR );
    int memID3 = shmget(IPC_PRIVATE, memSize3, S_IRUSR | S_IWUSR );

    // Clean exit if unable to create the shared memory
    if(memID1 < 0 || memID2 < 0 || memID3 < 0) {
        errmsg("Error creating shared memory segment.\n");
    }

    /* Init arrays and barrier and attach to shared memory*/
    int *inArray = (int*)shmat(memID1, NULL, 0); // shmat returns a pointer to the shared memory segment
    int *outArray = (int*)shmat(memID2, NULL, 0);
    int *barrier = (int*)shmat(memID3, NULL, 0);
    
    /* Init the barrier */
    *barrier = 0;

    /* Create the input array from the given input file */
    // count = the number of values read from the input file, or -1 if file was not able to open
    int count = makeInputArray(infileName, inArray, arrSize);

    /* Clean exit if unable to open the input file or not enough input values */
    if(count < 0 || count < (arrSize-1)) {
        removeMemory(memID1, memID2, memID3, inArray, outArray, barrier); // Remove the shared memory
        errmsg("Invalid input file.\n");
    }

    // Calculate the number of iterations needed
    int iterations = floor(log2f(arrSize));

    // Calculate the size of each block (index range per process)
    int blockSize = round((float)arrSize/(float)numProcesses);
    int status = 0; // For waiting for child processes

    /* Create the processes */
    for(int j = 0 ; j < numProcesses ; j++) {
        if(fork() == 0) {
            // Child process begins here
            // j is the process number
            for(int i = 0 ; i <= iterations ; i++) {
                parallelScan(inArray, outArray, j, numProcesses, i, blockSize, arrSize); // Compute for this iteration
                synchronize(barrier, j, i, numProcesses); // synchronize all processes
                swapArrays(inArray, outArray); // swap the arrays
            }
            // Child process ends
            exit(0);
        }
    }
    // Parent wait for children to complete
    while(wait(&status) > 0);

    // If iterations is odd, swap the arrays
    if((iterations % 2) != 0) swapArrays(inArray, outArray);

    // Write the result to the output file
    // Clean exit if unable to open the output file
    if(writeOutputArray(outfileName, outArray, arrSize) < 0) {
        removeMemory(memID1, memID2, memID3, inArray, outArray, barrier);
        errmsg("Unable to open the output file.\n");
    }

    // Detach from shared memory and remove shared memory segment
    removeMemory(memID1, memID2, memID3, inArray, outArray, barrier);

    return 0;
}
