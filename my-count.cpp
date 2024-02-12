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
// @param msg: String to be printed
void errmsg(string msg) {
    cerr << msg;
    exit(1);
}

int main(int argc, char* argv[]) {

    int numArgs = 5; // Values N, M, input file string, output file string

    /* Clean exit if not enough args */
    if(argc < numArgs) {
        errmsg("Not enough arguments provided.\n");
    }
    
    // Temporarily store the first two arguments
    string N = argv[1];
    string M = argv[2];

    /* Clean exit if first two arguments are not positive integers */
    for (int i = 0; i < N.length() ; i++) {
        if(!isdigit(N[i])) {
            errmsg("Invalid arguments provided.\n");
        }
    }

    for (int i = 0; i < M.length() ; i++) {
        if(!isdigit(M[i])) {
            errmsg("Invalid arguments provided.\n");
        }
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
    
    /* Init test arrays and attach to shared memory */
    int *sumArr = memPtr; // Input at location 0
    int *buffer = memPtr + sizeof(int)*arrSize; // Temp array at next location

    /* Create the input array from the given input file */
    /* Open the input file */
    ifstream inFile;
    inFile.open(infileName);

    /* Clean exit if unable to open the input file */
    if(!inFile.is_open()) {
        // Detach from shared memory
        shmdt((void*) memPtr);
        // Remove the shared memory segment
        shmctl(memID, IPC_RMID, NULL);
        errmsg("Unable to open the input file.\n");
    }

    int count = 0; // To track how many input values have been read
    int currVal; // To hold the current input value
    while(inFile >> currVal) { // Read from file
        if(count >= arrSize) { break; } // Do not continue if out of the given range
        sumArr[count] = currVal; // Store in the shared memory array
        count++;
    }
    // Close the file
    inFile.close();

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
    // To be used in the algorithm
    int blockStart;
    int blockEnd;
    int algVal;

    int status = 0; // For waiting for child processes

    /* Create all the child processes and perform the algorithm here */
    for(int i = 0 ; i <= iterations; i++) {
        for(int j = 0 ; j < numProcesses ; j++) { 
            // j is the process number, i is the iteration
            // For all iterations, "sumArr" is the current iteration array, "buffer" is next iteration array
            if(fork() == 0) {
                /* Child process begin here */
                algVal = (int)pow(2, i); // Used for algorithm

                // Determine range for process
                blockStart = j*blockSize;
                blockEnd = blockStart + blockSize;

                // If it's the last process, end at arrSize
                if(j == (numProcesses-1)) {
                    blockEnd = arrSize;
                }

                // Hillis and Steele algorithm
                for(int k = blockStart ; k < blockEnd ; k++) {
                    if(k >= arrSize) { break; } // Out of range, don't calculate
                    if(k < algVal) {
                        buffer[k] = sumArr[k]; // Same value copied for next iteration
                    }
                    else {
                        buffer[k] = sumArr[k] + sumArr[k - algVal];
                    }
                }
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
    // Final result is in sumArr
    /* Open the output file */
    ofstream outFile;
    outFile.open(outfileName);

    /* Clean exit if unable to open the output file */
    if(!outFile.is_open()) {
        // Detach from shared memory
        shmdt((void*) memPtr);
        // Remove the shared memory segment
        shmctl(memID, IPC_RMID, NULL);
        errmsg("Unable to open the output file.\n");
    }

    /* Write the result to the output file */
    for(int i = 0 ; i < arrSize ; i++) {
        outFile << sumArr[i] << '\n';
    }
    outFile.close();

    // Detach from shared memory
    shmdt((void*) memPtr);

    // Remove the shared memory segment
    shmctl(memID, IPC_RMID, NULL);

    return 0;
}
