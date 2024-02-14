/*

Authors: Meghan Grayson and Vaishnavi Karaguppi
Date: February 11, 2024

This program computes a prefix sum with Hillis and Steele's parallel algorithm.
The specified number of processes are created with each iteration while the parent process waits for them to terminate.
It uses two arrays for space efficiency.

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

using namespace std;

/* Handle errors and bad input */
// msg -- String to be printed
void errmsg(string msg) {
    cerr << msg;
    exit(1);
}

/* Determine if input values for N and M are valid */
// argCount -- number of arguments provided to main
// args -- array containing the arguments provided to main as strings
//
// return -- 0 if valid, -1 if not valid
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
    for(int i = 0; i < N.length(); i++) {
        if(!isdigit(N[i])) {
            return -1;
        }
    }
    for(int i = 0; i < M.length(); i++) {
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

/* Create the input array from the given input file */
// filename -- the name of the input file
// array -- the location for the values to be stored
// N -- The length of the input array
//
// return -- the number of values written, or -1 if unable to open the file
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

/* Create the output file from the given array */
// filename -- the name of the output file
// array -- the location of the values to be written
// N -- The length of the output array
//
// return -- 0 if successful, or -1 if unable to open the file
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

/* Perform the Hillis and Steele algorithm, executed by child processes */
// arr1 -- array for the "current" iteration
// arr2 -- array for the "next" iteration
// processNum -- the number associated with the current process
// processes -- the total number of processes
// iter -- the current iteration
// blockSize -- the number of elements to be handled by the current process
// arraySize -- the size/length of the arrays
void parallelScan(int* arr1, int* arr2, int processNum, int processes, int iter, int blockSize, int arraySize) {
    // Determine the range for the current process
    int blockStart = processNum * blockSize;
    int blockEnd = blockStart + blockSize;

    // If current process is the last one, blockEnd should be the last element
    if(processNum == (processes -1)) { blockEnd = arraySize; }

    int temp = (int)pow(2, iter); // value to be used in the algorithm

    // Perform algorithm
    for(int k = blockStart ; k < blockEnd ; k++) {
        if(k >= arraySize) { break; } // Out of range, don't calculate
        if(k < temp) {
            arr2[k] = arr1[k]; // Same value copied for next iteration
        }
        else {
            arr2[k] = arr1[k] + arr1[k - temp];
        }
    }
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

    /* Create the shared memory segment */
    int memID = shmget(IPC_PRIVATE, sizeof(int)*arrSize*2, 0666|IPC_CREAT);
    // Clean exit if unable to create the shared memory
    if(memID < 0) {
        errmsg("Error creating shared memory segment.\n");
    }
    int *memPtr = (int*)shmat(memID, NULL, 0); // shmat returns a pointer to the shared memory segment
    
    /* Init arrays and barrier and attach to shared memory */
    int *sumArr = memPtr; // Input at location 0
    int *buffer = memPtr + sizeof(int)*arrSize; // Temp array at next location

    /* Create the input array from the given input file */
    // count = the number of values read from the input file, or -1 if file was not able to open
    int count = makeInputArray(infileName, sumArr, arrSize);

    /* Clean exit if unable to open the input file */
    if(count < 0) {
        // Detach from shared memory
        shmdt((void*) memPtr);
        // Remove the shared memory segment
        shmctl(memID, IPC_RMID, NULL);
        errmsg("Unable to open the input file.\n");
    }

    /* Clean exit if not enough input values */
    if(count < (arrSize-1)) {
        // Detach from shared memory
        shmdt((void*) memPtr);
        // Remove the shared memory segment
        shmctl(memID, IPC_RMID, NULL);
        errmsg("Not enough values in the input file.\n");
    }

    // Calculate the number of iterations needed
    int iterations = floor(log2f(arrSize));

    // Calculate the size of each block (index range per process)
    int blockSize = round((float)arrSize/(float)numProcesses);
    int status = 0; // For waiting for child processes

    /* Create all the child processes and perform the algorithm here */
    for(int i = 0 ; i <= iterations; i++) {
        for(int j = 0 ; j < numProcesses ; j++) { 
            // j is the process number, i is the iteration
            // For all iterations, "sumArr" is the current iteration array, "buffer" is next iteration array
            if(fork() == 0) {
                /* Child process begin here */
                parallelScan(sumArr, buffer, j, numProcesses, i, blockSize, arrSize);
                // Child process ends
                exit(0);
            }
        }
        // Wait for all children to complete
        while(wait(&status) > 0);

        // Swap the buffer arrays
        int* temp = sumArr;
        sumArr = buffer;
        buffer = temp;
    }

    // Write the result to the output file
    // Clean exit if unable to open the output file
    if(writeOutputArray(outfileName, sumArr, arrSize) < 0) {
        // Detach from shared memory
        shmdt((void*) memPtr);
        // Remove the shared memory segment
        shmctl(memID, IPC_RMID, NULL);
        errmsg("Unable to open the output file.\n");
    }

    // Detach from shared memory
    shmdt((void*) memPtr);

    // Remove the shared memory segment
    shmctl(memID, IPC_RMID, NULL);

    return 0;
}
