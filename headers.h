#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <math.h>
#include <limits.h>

typedef short bool;
#define true 1
#define false 0
#define MEM_SIZE 1024    // the starting size given for the memory
#define MIN_BLOCK_SIZE 2 // minimum size
#define SHKEY 300

///==============================
// don't mess with this variable//
int *shmaddr; //
//===============================

int getClk()
{
    return *shmaddr;
}

/*
 * All processes call this function at the beginning to establish communication between them and the clock module.
 * Again, remember that the clock is only emulation!
 */
void initClk()
{
    int shmid = shmget(SHKEY, 4, 0444);
    while ((int)shmid == -1)
    {
        // Make sure that the clock exists
        printf("Wait! The clock is not initialized yet!\n");
        sleep(1);
        shmid = shmget(SHKEY, 4, 0444);
    }
    shmaddr = (int *)shmat(shmid, (void *)0, 0);
}

/*
 * All processes call this function at the end to release the communication
 * resources between them and the clock module.
 * Again, Remember that the clock is only emulation!
 * Input: terminateAll: a flag to indicate whether this is the end of simulation.
 *                      It terminates the whole system and releases resources.
 */

void destroyClk(bool terminateAll)
{
    shmdt(shmaddr);
    if (terminateAll)
    {
        killpg(getpgrp(), SIGINT);
    }
}
///////////////////////////////////////////////////////////////////
////////////////////////PROCESS STRUCTURES/////////////////////////
///////////////////////////////////////////////////////////////////
struct processInputData
{
    int arrivalTime;
    int priority;
    int runningTime;
    int id;
    int memSize; // ADDED
};

struct processInfo
{
    int id;
    int arrivalTime;
    int runningTime;
    int priority;
    int PID;
    char processState[10];
    int remainingTime;
    int finishedTime;
    int waitingTime;
    int turnAroundTime;
    float weightedTurnAroundTime;
    int memSize;       // ADDED
    int startPosition; // ADDED
    int endPosition;   // ADDED
};

int startProccess(int runTime)
{
    char temp[100];
    sprintf(temp, "%d", runTime);
    const int PID = fork();
    if (PID == -1)
    {
        perror("error while forking a process");
    }
    else if (PID == 0)
    {
        // execl(PROCESS_PATH, "process.out", temp, NULL);
    }
    return PID;
}
/////////////////////////////////////////////////////////////
/////////////////////NODE IMPLEMENTATION/////////////////////
/////////////////////////////////////////////////////////////
struct node
{
    struct processInfo *p;
    struct node *next;
};

struct node *newNode(struct processInfo process)
{
    struct node *temp = malloc(sizeof(struct node));
    struct processInfo *arrivingProcess = malloc(sizeof(struct processInfo));
    arrivingProcess->id = process.id;
    arrivingProcess->arrivalTime = process.arrivalTime;
    arrivingProcess->runningTime = process.runningTime;
    arrivingProcess->priority = process.priority;
    arrivingProcess->PID = process.PID;
    strcpy(arrivingProcess->processState, process.processState);
    arrivingProcess->remainingTime = process.remainingTime;
    arrivingProcess->finishedTime = process.finishedTime;
    arrivingProcess->waitingTime = process.waitingTime;
    arrivingProcess->turnAroundTime = process.turnAroundTime;
    arrivingProcess->weightedTurnAroundTime = process.weightedTurnAroundTime;
    arrivingProcess->memSize = process.memSize;
    arrivingProcess->startPosition = process.startPosition;
    arrivingProcess->endPosition = process.endPosition;
    temp->p = arrivingProcess;
    temp->next = NULL;
    return temp;
}
/////////////////////////////////////////////////////////////
////////////////PRIORITY QUEUE IMPLEMENTATION////////////////
/////////////////////////////////////////////////////////////
struct priorityQueue
{
    struct node *head;
    int count;
};

struct priorityQueue *createPriorityQueue()
{
    struct priorityQueue *pq = malloc(sizeof(struct priorityQueue));
    if (pq != NULL)
    {
        pq->head = NULL;
        pq->count = 0;
    }
    return pq;
}

bool IsEmpty(struct priorityQueue *pq)
{
    return pq->head == NULL;
}

struct processInfo *peekQueue(struct priorityQueue *pq)
{
    if (pq == NULL || pq->head == NULL)
    {
        return NULL;
    }
    return pq->head->p;
}

// DEQUEUE FUNCTION
struct processInfo *dequeue(struct priorityQueue *pq)
{
    if (pq == NULL || pq->head == NULL)
    {
        return NULL;
    }

    struct node *frontNode = pq->head;
    pq->head = frontNode->next;

    struct processInfo *frontProcess = frontNode->p;
    free(frontNode);
    pq->count--;

    return frontProcess;
}

// ENQUEUE FUNCTION
void enqueue(struct priorityQueue *Queue, struct processInfo processToBeAdded, int algorithmUsed)
{
    if (Queue == NULL)
    {
        return;
    }

    struct node *temp = newNode(processToBeAdded);
    if (temp == NULL)
    {
        return;
    }

    if (Queue->head == NULL)
    {
        Queue->head = temp;
        Queue->count = 1;
        return;
    }

    if (algorithmUsed == 1)
    {
        // HPF PROCESS
        if (Queue->head->p->priority > processToBeAdded.priority)
        {
            temp->next = Queue->head;
            Queue->head = temp;
        }
        else
        {
            struct node *start = Queue->head;
            while (start->next != NULL && start->next->p->priority <= processToBeAdded.priority)
            {
                start = start->next;
            }
            temp->next = start->next;
            start->next = temp;
        }
    }
    else if (algorithmUsed == 2)
    {
        // SRTN PROCESS
        if (Queue->head->p->remainingTime > processToBeAdded.remainingTime)
        {
            temp->next = Queue->head;
            Queue->head = temp;
        }
        else
        {
            struct node *start = Queue->head;
            while (start->next != NULL && start->next->p->remainingTime <= processToBeAdded.remainingTime)
            {
                start = start->next;
            }
            temp->next = start->next;
            start->next = temp;
        }
    }
    else
    {
        // RR PROCESS
        struct node *start = Queue->head;
        while (start->next != NULL)
        {
            start = start->next;
        }
        start->next = temp;
    }

    Queue->count++;
}
/////////////////////////////////////////////////////////////
////////////////////////IPC RESOURCES////////////////////////
/////////////////////////////////////////////////////////////

// Message structure for IPC
struct msgbuff
{
    long mType;
    struct processInfo mprocess;
};

struct msgProcess
{
    long mType;
    bool done;
};

union Semun
{
    int val;               /* Value for SETVAL */
    struct semid_ds *buf;  /*Buffer for IPC_STAT, IPC_SET */
    unsigned short *array; /* Array for GETALL, SETALL */
    struct seminfo *__buf; /*Buffer for IPC_INFO (Linux-specific) */
    void *__pad;
};
void down(int sem)
{
    struct sembuf op;

    op.sem_num = 0;
    op.sem_op = -1;
    op.sem_flg = !IPC_NOWAIT;

    if (semop(sem, &op, 1) == -1)
    {
        perror("Error in down()");
        exit(-1);
    }
}
////////////////////////////////////////////////////////////////
////////////////////////PRINTING SECTION////////////////////////
///////////////////////////////////////////////////////////////

void printSingleProcess(struct processInfo *Process)
{
    printf("ID of the process is: %d\n", Process->id);
    printf("Arrival time of the process is: %d\n", Process->arrivalTime);
    printf("Running time of the process is: %d\n", Process->runningTime);
    printf("Priority of the process is: %d\n", Process->priority);
    printf("PID of the process is: %d\n", Process->PID);
    printf("State of the process is: %s\n", Process->processState);
    printf("Remaining time of the process is: %d\n", Process->remainingTime);
    printf("Finished time of the process is: %d\n", Process->finishedTime);
    printf("Waiting time of the process is: %d\n", Process->waitingTime);
    printf("Turn around time of the process is: %d\n", Process->turnAroundTime);
    printf("Weighted turn around time of the process is: %f\n", Process->weightedTurnAroundTime);
}

void printQueue(struct priorityQueue *Queue)
{
    if (Queue == NULL)
    {
        printf("Queue is not initialized.\n");
        return;
    }

    if (IsEmpty(Queue))
    {
        printf("Queue is empty.\n");
        return;
    }

    printf("Queue contents:\n");
    struct node *temp = Queue->head;
    while (temp != NULL)
    {
        struct processInfo *proc = temp->p;
        printf("Process ID: %d, Arrival Time: %d, Running Time: %d, Priority: %d, Remaining Time: %d\tsize: \t%d\tstart: \t%d\tend: \t%d\n",
               proc->id, proc->arrivalTime, proc->runningTime, proc->priority, proc->remainingTime, proc->memSize, proc->startPosition, proc->endPosition);
        temp = temp->next;
    }
}

//////////////////////////////////////////////////////////////////
////////////////////////MEMORY ALLOCATION////////////////////////
/////////////////////////////////////////////////////////////////
struct memoryNode
{
    int size;
    int pid;
    bool emptyNode;
    bool hasProcess;
    bool hasChild;
    bool isRoot;
    int startPosition;
    int endPosition;
    struct memoryNode *parent;
    struct memoryNode *left;
    struct memoryNode *right;
};

struct memoryTree
{
    struct memoryNode *root;
};

struct memoryNode *createMemoryNode(int size, int startPosition, struct memoryNode *parent)
{
    struct memoryNode *node = (struct memoryNode *)malloc(sizeof(struct memoryNode));
    if (node == NULL)
    {
        return NULL;
    }

    node->size = size;
    node->pid = -1; // no process yet is created
    node->emptyNode = true;
    node->hasProcess = false;
    node->hasChild = false;
    if (parent == NULL)
    {
        node->isRoot = true;
    }
    else
    {
        node->isRoot = false;
    }
    node->startPosition = startPosition;
    node->endPosition = startPosition + size - 1;
    node->parent = parent;
    node->left = NULL;
    node->right = NULL;

    return node;
}
struct memoryTree *createMemoryTree()
{
    struct memoryTree *tree = (struct memoryTree *)malloc(sizeof(struct memoryTree));
    if (tree == NULL)
    {
        // for memory allocation failure
        return NULL;
    }
    // used create node function to create the node
    tree->root = createMemoryNode(MEM_SIZE, 0, NULL);

    // this if for the case of failure to create node then memory previously allocated for the tree itself is no longer needed bec there is no root
    if (tree->root == NULL)
    {
        // for memory allocation failure
        free(tree);
        return NULL;
    }
    return tree;
}

struct memoryNode *findProcessNode(struct memoryNode *current, int targetPid)
{
    // check if the current node is null
    if (current == NULL)
    {
        return NULL;
    }

    // returning this node if it has the matching process ID
    if (current->pid == targetPid)
    {
        return current;
    }

    // traverse through the left subtree
    struct memoryNode *foundInLeft = findProcessNode(current->left, targetPid);
    if (foundInLeft != NULL)
    {
        return foundInLeft;
    }

    // traverse through the right subtree
    return findProcessNode(current->right, targetPid);
}

struct memoryNode *findSuitableSize(struct memoryNode *current, int requiredSize)
{
    // i am here checking if the current code is equal to null or if the required size to be allocated is smaller than the current size of the node
    if (current == NULL || current->size < requiredSize)
    {
        return NULL;
    }

    struct memoryNode *leftNode = findSuitableSize(current->left, requiredSize);
    // printf("LEFT NODE SIZE %d\n",leftNode->size);
    struct memoryNode *rightNode = findSuitableSize(current->right, requiredSize);

    // i am here checking if the node exists and then i check if the size of this node is greater than the required size and if this current node is empty
    int leftSize = (leftNode != NULL && leftNode->size >= requiredSize && leftNode->emptyNode && leftNode->left == NULL && leftNode->right == NULL) ? leftNode->size : INT_MAX;
    int rightSize = (rightNode != NULL && rightNode->size >= requiredSize && rightNode->emptyNode && rightNode->left == NULL && rightNode->right == NULL) ? rightNode->size : INT_MAX;
    printf("SHEMALLLL %d WE YEMEEEN %d\n", leftSize, rightSize);

    if (leftSize <= rightSize && leftSize < current->size)
    {
        return leftNode;
    }
    else if (rightSize < leftSize && rightSize < current->size)
    {
        printf("ANA MO8AFAAAAL\n");
        return rightNode;
    }
    printf("STATUS OF FOUND NODE MN GOWAAA %d WE BDAYTY %d  WE NHAYTYY  %d\n", current->hasProcess, current->startPosition, current->endPosition);
    if (current->hasProcess == 1)
    {
        printf("MANFOO5A MN GOWAAA\n");
        return NULL;
    }
    else
    {
        printf("BARAGA3\n");
        return current;
    }
}

struct memoryNode *findClosestSize(struct memoryTree *tree, int size)
{
    float log2Size = log2(size);
    int wantedSize = pow(2, ceil(log2Size));
    if (size <= 8)
    {
        wantedSize = 8;
    }
    printf("WANTED SIZE %d \n", wantedSize);
    struct memoryNode *foundNode = findSuitableSize(tree->root, wantedSize);
    // printf("STARTING POSITION OF FOUND NODE %d \n",foundNode->startPosition);
    printf("STATUS OF FOUND NODE %d \n", foundNode->hasProcess);
    if (foundNode == tree->root && foundNode->hasChild == true)
    {
        printf("KOLOOO 3LA A5ROO\n");
        return NULL;
    }
    while (foundNode->size != wantedSize)
    {
        printf("SIZE OF FOUND NODE %d \n", foundNode->size);
        struct memoryNode *leftNode = createMemoryNode(foundNode->size / 2, foundNode->startPosition, foundNode);
        struct memoryNode *rightNode = createMemoryNode(foundNode->size / 2, foundNode->startPosition + foundNode->size / 2, foundNode);
        leftNode->emptyNode = true;
        rightNode->emptyNode = true;
        foundNode->left = leftNode;
        foundNode->right = rightNode;
        foundNode->hasChild = true;

        foundNode = findSuitableSize(tree->root, wantedSize);
    }

    return foundNode;
}

int *allocateProcess(struct memoryTree *tree, int processSize, int pid)
{
    struct memoryNode *foundNode = findClosestSize(tree, processSize);
    int *positions = (int *)malloc(sizeof(int));

    if (foundNode == NULL)
    {
        positions[0] = -1;
        positions[1] = -1;
        return positions;
    }
    printf("ANA FEYA WALA            %d\n", foundNode->hasProcess);
    foundNode->pid = pid;
    foundNode->emptyNode = false;
    foundNode->hasProcess = true;
    positions[0] = foundNode->startPosition;
    positions[1] = foundNode->endPosition;
    printTree(tree->root);
    return positions;
}

void deleteChildren(struct memoryNode *node)
{
    if (node->left)
    {
        free(node->left);
        node->left = NULL;
    }
    if (node->right)
    {
        free(node->right);
        node->right = NULL;
    }
    node->hasChild = false;
    node->emptyNode = true;
}

void mergeMemory(struct memoryNode *node)
{
    struct memoryNode *currentNode = node;

    // while (currentNode)
    // {
    printf("BOS YASTA %d \n", currentNode->right->emptyNode);
    if (currentNode->left && !currentNode->left->hasChild && !currentNode->left->hasProcess && currentNode->right && !currentNode->right->hasChild && !currentNode->right->hasProcess)
    {
        printf("ANA HENA\n");
        deleteChildren(currentNode);
    }

    if (!currentNode->isRoot)
        currentNode = currentNode->parent;
    else
        return;
    // }
}

int deallocateProcess(struct memoryTree *tree, int pid)
{
    struct memoryNode *foundNode = findProcessNode(tree->root, pid);
    printf("ANA SHAYELLL %d \n", foundNode->pid);
    if (foundNode == NULL)
        return -1;

    foundNode->pid = -1;
    foundNode->emptyNode = true;
    foundNode->hasProcess = false;

    mergeMemory(foundNode->parent);

    return 1;
}

void printTree(struct memoryNode *root)
{
    if (root == NULL)
        return;

    printTree(root->left); // traverse through the Left Part
    printf("Size: %d, PID: %d, Start: %d, End: %d, HAS PROCESS: %d\n", root->size, root->pid, root->startPosition, root->startPosition + root->size, root->hasProcess);
    printTree(root->right); // traverse through the right part after finishing all the left part
}
