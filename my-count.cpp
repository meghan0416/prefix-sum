/*

Compile: g++ -o myCount my-count.cpp
Run: ./myCount N M

Where N = size of the array, M = number of processes

*/


#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <sys/ipc.h>
#include <sys/shm.h>

using namespace std;

/* Handle errors */
void errmsg(string msg) {
    cerr << msg;
    exit(0);
}

int main(int argc, char* argv[]) {

    int numArgs = 2; // Testing with only values N and M for now

    /* Clean exit if not enough args */
    if(argc < (numArgs+1)) {
        errmsg("Not enough arguments provided.\n");
    }
    
    int arrSize = atoi(argv[1]);
    int numProcesses = atoi(argv[2]);

    /* Clean exit if args nonsensible */
    if((numProcesses < 0) || (arrSize < 0)) {
        errmsg("Invalid arguments provided.\n");
    }

    /* Create the shared memory segment */
    int key = 65; // key is arbitrary
    int memID = shmget(key, sizeof(int)*arrSize*2, 0666|IPC_CREAT);
    int *memPtr = (int*)shmat(memID, NULL, 0); // shmat returns a pointer to the shared memory segment
    
    /* Init test arrays and attach to shared memory */
    int *sumArr = memPtr; // Input at location 0
    int *Buffer = memPtr + sizeof(int)*arrSize; // Temp array at next location

    for(int i = 0; i < arrSize ; i++) { 
        sumArr[i] = 1;
    } // Filled with all 1s for testing

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

                for(int k = blockStart ; k < blockEnd ; k++) {
                    if(k >= arrSize) { break; } // out of range, don't calculate

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

        // Print contents of buffers

        // Swap the buffer arrays
        int* temp = sumArr;
        sumArr = Buffer;
        Buffer = temp;
    }

    // Final result is in sumArr
    // Print the resulting array
    cout << "Result ----\n";
    for(int i = 0 ; i < arrSize ; i++) {
        cout << sumArr[i] << " ";
    }
    cout << endl;

    // Detach from shared memory
    shmdt((void*) sumArr);
    shmdt((void*) Buffer);

    // Remove the shared memory segment
    shmctl(memID, IPC_RMID, NULL);

    return 0;
}
