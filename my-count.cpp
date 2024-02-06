/*

Compile: g++ -o myCount my-count.cpp
Run: ./prefixSum N M A.txt B.txt

Where N = size of the array, M = number of processes, A.txt = the input filename, B.txt = the output filename

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
    exit(0);
}

int main(int argc, char* argv[]) {

    int numArgs = 4; // Values N, M, input file string, output file string

    /* Clean exit if not enough args */
    if(argc < (numArgs+1)) {
        errmsg("Not enough arguments provided.\n");
    }
    
    /* Assign the input args */
    int arrSize = atoi(argv[1]);
    int numProcesses = atoi(argv[2]);
    string infileName = argv[3];
    string outfileName = argv[4];

     /* Clean exit if args nonsensible or can't open the file */
    if((numProcesses < 0) || (arrSize < 0)) {
        errmsg("Invalid arguments provided.\n");
    }

    /* If there are more cores than N, no need to make additional processes */
    if(numProcesses > arrSize) { numProcesses = arrSize; }

    /* Open the files */
    ifstream inFile;
    inFile.open(infileName);

    /* Clean exit if unable to open the files*/
    if(!inFile.is_open()) {
        errmsg("Unable to open the input file.\n");
    }

    ofstream outFile;
    outFile.open(outfileName);

    if(!outFile.is_open()) {
        inFile.close(); // Close the input file
        errmsg("Unable to open the output file.\n");
    }

    /* Create the shared memory segment */
    int key = 65; // key is arbitrary
    int memID = shmget(key, sizeof(int)*arrSize*2, 0666|IPC_CREAT);
    int *memPtr = (int*)shmat(memID, NULL, 0); // shmat returns a pointer to the shared memory segment
    
    /* Init test arrays and attach to shared memory */
    int *sumArr = memPtr; // Input at location 0
    int *Buffer = memPtr + sizeof(int)*arrSize; // Temp array at next location

    /* Create the input array from the given input file */
    int count = 0;
    int currVal;
    while(inFile >> currVal) {
        if(count >= arrSize) { break; } // Do not continue if out of the given range
        sumArr[count] = currVal;
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

        // Terminate gracefully if not enough values in the file
        errmsg("Not enough values in the input file.\n");
    }

    cout << "Input ----\n";
    for(int i = 0 ; i < arrSize ; i++) {
        cout << sumArr[i] << " ";
    }
    cout << endl;

    // Calculate the number of iterations needed
    int iterations = floor(log2f(arrSize));

    // Calculate the size of each block (index range per process)
    int blockSize = round((float)arrSize/(float)numProcesses);
    int blockStart;
    int blockEnd;
    int algVal;

    int status = 0; // For waiting for child processes

    for(int i = 0 ; i <= iterations; i++) {
        for(int j = 0 ; j < numProcesses ; j++) { 
            // j is the process number, i is the iteration
            // For all iterations, "bufferOne" is the current iteration array, "bufferTwo" is next iteration array
            if(fork() == 0) {

                /* Child process */
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
                        Buffer[k] = sumArr[k]; // Same value copied for next iteration
                    }
                    else {
                        Buffer[k] = sumArr[k] + sumArr[k - algVal];
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
        sumArr = Buffer;
        Buffer = temp;
    }

    // Final result is in sumArr

    /* Write the result to the output file */
    for(int i = 0 ; i < arrSize ; i++) {
        outFile << sumArr[i] << '\n';
    }
    outFile.close();

    // Print the resulting array
    cout << "Result ----\n";
    for(int i = 0 ; i < arrSize ; i++) {
        cout << sumArr[i] << " ";
    }
    cout << endl;

    // Detach from shared memory
    shmdt((void*) memPtr);

    // Remove the shared memory segment
    shmctl(memID, IPC_RMID, NULL);

    return 0;
}
