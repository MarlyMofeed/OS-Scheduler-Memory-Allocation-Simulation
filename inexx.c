#include <stdio.h>
#include <stdlib.h>
#include "headers.h" // Replace with the name of your header file

int main()
{
    // Create a memory tree
    struct memoryTree *tree = createMemoryTree();
    if (tree == NULL)
    {
        printf("Memory tree creation failed\n");
        return 1;
    }

    // Allocate memory for a process
    int pid1 = 1;
    int processSize1 = 100;
    int startPosition1 = allocateProcess(tree, processSize1, pid1);
    printf("Allocated PID %d, Start Position: %d\n", pid1, startPosition1);

    // Allocate another process
    int pid2 = 2;
    int processSize2 = 200;
    int startPosition2 = allocateProcess(tree, processSize2, pid2);
    printf("Allocated PID %d, Start Position: %d\n", pid2, startPosition2);

    // Deallocate a process
    int deallocResult = deallocateProcess(tree, pid1);
    if (deallocResult != -1)
    {
        printf("Deallocated PID %d\n", pid1);
    }

    // Print the tree structure
    printTree(tree->root);

    // Clean up
    free(tree); // Add additional cleanup if necessary for your implementation
    return 0;
}
