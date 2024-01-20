#include "headers.h"

#define HPF_ALGO 1
#define SRTN_ALGO 2
#define RR_ALGO 3

FILE *memoryLogFile;
FILE *schduelerLogFile; // this file will have the output of the schedular
FILE *perfFile;         // this file to have the output of calculations like the utilization and average
int quantum;            // Time unit given to each process in each cycle
int procressCount;
int tempQuantum;
int startingTimeOfTheProcess = 0;
bool processSending = true;                     // to check if there is still another process to be sent finished
struct processInfo *receivedProcess = NULL;     // Pointer used to currently received process
struct priorityQueue *receivedProcesses = NULL; // Priority Queue holding all received processes at a moment
struct processInfo *runningProcess = NULL;      // Pointer used to currently running process
struct priorityQueue *calcQueue = NULL;         // Priority Queue holding all finished processes at a moment to be used in calculations
struct priorityQueue *waitingProcesses = NULL;  // Priority Queue holding all processes that aren't in the MAIN MEMORY
struct memoryTree *tree = NULL;                 // Binary Search Tree Representing MAIN MEMORY
char *PRO_PATH;
int finishedProcesses = 0;
int processReceivedCount = 0;
char remainingTimeString[12];
char quantumStr[12];
int countActive = 0;
union Semun semun;
int rec_val, msgID, msgID2, send, countFinishedProcesses = 0;
struct msgbuff msg;
struct msgProcess processMsg;
int sem_sync = -1;
bool firstProcess = true;
int startTime = 0;
int endTime = 0;

/////////////////////////////////////////////////////////////////////////////
////////////////////////FILE PRINTING FUNCTIONALITIES////////////////////////
/////////////////////////////////////////////////////////////////////////////

// Responsible for Writing the Scheduler.log file
void schduelerLog(struct processInfo *processToPrint)
{
    if (processToPrint)
    {
        if (!strcmp(processToPrint->processState, "finished"))
        {
            fprintf(schduelerLogFile, "At\ttime\t%d\tprocess\t%d\t%s\tarr\t%d\ttotal\t%d\tremain\t%d\twait\t%d\tTA\t%d\tWTA\t%.2f\n", getClk(), processToPrint->id, processToPrint->processState, processToPrint->arrivalTime, processToPrint->runningTime, processToPrint->remainingTime, processToPrint->waitingTime, processToPrint->turnAroundTime, processToPrint->weightedTurnAroundTime);
        }
        else
        {
            fprintf(schduelerLogFile, "At\ttime\t%d\tprocess\t%d\t%s\tarr\t%d\ttotal\t%d\tremain\t%d\twait\t%d\n", getClk(), processToPrint->id, processToPrint->processState, processToPrint->arrivalTime, processToPrint->runningTime, processToPrint->remainingTime, processToPrint->waitingTime);
        }
    }
    else
    {
        return;
    }
}

// Responsible for Writing the Memory.log file
void memoryLog(struct processInfo *processToPrint)
{
    if (processToPrint)
    {
        if (!strcmp(processToPrint->processState, "arrived"))
        {
            fprintf(memoryLogFile, "At\ttime\t%d\tallocated\t%d\tbytes\tfor\tprocess\t%d\tfrom\t%d\tto\t%d\n", getClk(), processToPrint->memSize, processToPrint->id, processToPrint->startPosition, processToPrint->endPosition);
        }
        else if (!strcmp(processToPrint->processState, "finished"))
        {
            fprintf(memoryLogFile, "At\ttime\t%d\tfreed\t%d\tbytes\tfor\tprocess\t%d\tfrom\t%d\tto\t%d\n", getClk(), processToPrint->memSize, processToPrint->id, processToPrint->startPosition, processToPrint->endPosition);
        }
    }
    else
    {
        return;
    }
}

void receiveFromProcGen(int algorithmUsed)
{
    while (msgrcv(msgID, &msg, sizeof(msg.mprocess), 0, IPC_NOWAIT) != -1)
    {
        if (firstProcess)
        {
            startTime = getClk();
            firstProcess = false;
        }
        if (processReceivedCount == procressCount)
        {
            break;
        }
        processReceivedCount++;
        printf("Received process: %d\n", msg.mprocess.id);
        // Create a processInfo struct to store the received process
        receivedProcess = (struct processInfo *)malloc(sizeof(struct processInfo));
        // Copy the received process to the created struct
        *receivedProcess = msg.mprocess;
        strcpy(receivedProcess->processState, "arrived");

        // Allocate Process Memory HERE
        int *memoryPositions = allocateProcess(tree, receivedProcess->memSize, receivedProcess->id);
        if (memoryPositions[0] == -1 && memoryPositions[1] == -1)
        {
            printf("MEMORYYY MALYANAA AWYY YA SA7BY\n");
            printf("At\ttime\t%d\tprocess\t%d\t%s\tarr\t%d \ttotal\t%d\tremain\t%d\twait\t%d\tsize\t%d\tstart\t%d\tend\t%d\n", getClk(), receivedProcess->id, receivedProcess->processState, receivedProcess->arrivalTime, receivedProcess->runningTime, receivedProcess->remainingTime, receivedProcess->waitingTime, receivedProcess->memSize, receivedProcess->startPosition, receivedProcess->endPosition);
            enqueue(waitingProcesses, *receivedProcess, 3);
        }
        else
        {
            printf("ERKABB YA SA7BY\n");
            receivedProcess->startPosition = memoryPositions[0];
            receivedProcess->endPosition = memoryPositions[1];
            printf("At\ttime\t%d\tprocess\t%d\t%s\tarr\t%d \ttotal\t%d\tremain\t%d\twait\t%d\tsize\t%d\tstart\t%d\tend\t%d\n", getClk(), receivedProcess->id, receivedProcess->processState, receivedProcess->arrivalTime, receivedProcess->runningTime, receivedProcess->remainingTime, receivedProcess->waitingTime, receivedProcess->memSize, receivedProcess->startPosition, receivedProcess->endPosition);
            enqueue(receivedProcesses, *receivedProcess, algorithmUsed);
            memoryLog(receivedProcess);
        }
        // schduelerLog(receivedProcess);
        // Enqueue the received process to the receivedProcesses queue
        // printQueue(receivedProcesses);
        printTree(tree->root);
    }
    return;
}

int forkProcess()
{
    int PID = fork();
    if (PID == -1)
    {
        perror("error while forking a process");
    }
    else if (PID == 0)
    {
        // Execute the process.out file with the remaining time and the quantum in the child process
        char buffer[100];
        snprintf(remainingTimeString, sizeof(remainingTimeString), "%d", runningProcess->remainingTime);
        snprintf(quantumStr, sizeof(quantumStr), "%d", quantum);
        execl(PRO_PATH, "process.out", remainingTimeString, quantumStr, NULL);
    }
    return PID;
}

void checkAllocation(int algorithmUsed)
{
    struct priorityQueue *temp = createPriorityQueue();
    struct processInfo *processToAllocate = NULL;
    while (peekQueue(waitingProcesses))
    {
        processToAllocate = dequeue(waitingProcesses);
        if (processToAllocate)
        {
            int *memoryPositions = allocateProcess(tree, processToAllocate->memSize, processToAllocate->id);
            printf("BABDA2 MN %d\n", memoryPositions[0]);
            printf("BA5ALAS MN %d\n", memoryPositions[1]);
            if (memoryPositions[0] == -1 && memoryPositions[1] == -1)
            {
                printf("MEMORYYY MALYANAA AWYY YA SA7BY\n");
                enqueue(temp, *processToAllocate, 3);
            }
            else
            {
                printf("ERKABB YA SA7BY\n");
                receivedProcess = processToAllocate;
                receivedProcess->startPosition = memoryPositions[0];
                receivedProcess->endPosition = memoryPositions[1];
                enqueue(receivedProcesses, *receivedProcess, algorithmUsed);
                memoryLog(receivedProcess);
            }
        }
        else
        {
            printf("No Waiting Processes\n");
            break;
        }
    }
    while (peekQueue(temp))
    {
        processToAllocate = dequeue(temp);
        enqueue(waitingProcesses, *processToAllocate, 3);
    }
    return;
}
void HPF()
{
    printf("Running HPF Scheduler\n");
    // Check if there is a process to receive (non blocking)
    receiveFromProcGen(HPF_ALGO);
    // printQueue(receivedProcesses);
    // If there is no running process, try to dequeue one, and then fork it
    if (!runningProcess)
    {
        // Try to dequeue
        runningProcess = dequeue(receivedProcesses);
        if (runningProcess)
        {
            int PID = forkProcess();
            // In the shceduler process, set the PID of the running process to the forked process PID
            runningProcess->PID = PID;
            strcpy(runningProcess->processState, "started");
            printf("At\ttime\t%d\tprocess\t%d\t%s\tarr\t%d\ttotal\t%d\tremain\t%d\twait\t%d\tsize\t%d\tstart\t%d\tend\t%d\n", getClk(), runningProcess->id, runningProcess->processState, runningProcess->arrivalTime, runningProcess->runningTime, runningProcess->remainingTime, runningProcess->waitingTime, runningProcess->memSize, runningProcess->startPosition, runningProcess->endPosition);
            runningProcess->waitingTime = getClk() + runningProcess->remainingTime - (runningProcess->arrivalTime + runningProcess->runningTime); // ADDED
            schduelerLog(runningProcess);
            return;
        }
        // If nothing was dequeued, there is no process to run
        else
        {
            printf("NO RUNNING PROCESS\n");
        }
        return;
    }
    else
    {
        countActive++;
        // There is already a running process
        printf("Already running process: ID = %d\n", runningProcess->id);
        runningProcess->remainingTime--;
        processMsg.done = true;
        processMsg.mType = runningProcess->PID;
        // print pid of the process i am sending to
        printf("Sending to process -> PID = %d\n", runningProcess->PID);
        send = msgsnd(msgID2, &processMsg, sizeof(struct processInfo), !IPC_NOWAIT);
        if (runningProcess->remainingTime == 0)
        {
            // wait for the process to terminate
            waitpid(runningProcess->PID, NULL, 0);
            printf("Process %d terminated\n", runningProcess->id);
            strcpy(runningProcess->processState, "finished");
            runningProcess->finishedTime = getClk();
            runningProcess->turnAroundTime = runningProcess->finishedTime - runningProcess->arrivalTime;
            runningProcess->weightedTurnAroundTime = (float)(runningProcess->turnAroundTime) / runningProcess->runningTime;
            printf("At\ttime\t%d\tprocess\t%d\t%s\tarr\t%d\ttotal\t%d\tremain\t%d\twait\t%d\tsize\t%d\tstart\t%d\tend\t%d\n", getClk(), runningProcess->id, runningProcess->processState, runningProcess->arrivalTime, runningProcess->runningTime, runningProcess->remainingTime, runningProcess->waitingTime, runningProcess->memSize, runningProcess->startPosition, runningProcess->endPosition);
            schduelerLog(runningProcess);
            memoryLog(runningProcess);
            deallocateProcess(tree, runningProcess->id);
            checkAllocation(HPF_ALGO);
            enqueue(calcQueue, *runningProcess, 1);
            // Reset the running process pointer for the next iteration to dequeue a new process
            runningProcess = NULL;
            countFinishedProcesses++;
            if (countFinishedProcesses == procressCount)
                endTime = getClk();
            printf("Count Finished Processes = %d\n", countFinishedProcesses);
            runningProcess = dequeue(receivedProcesses);
            if (runningProcess)
            {
                int PID = forkProcess();

                // In the shceduler process, set the PID of the running process to the forked process PID
                runningProcess->PID = PID;
                strcpy(runningProcess->processState, "started");
                printf("At\ttime\t%d\tprocess\t%d\t%s\tarr\t%d\ttotal\t%d\tremain\t%d\twait\t%d\n", getClk(), runningProcess->id, runningProcess->processState, runningProcess->arrivalTime, runningProcess->runningTime, runningProcess->remainingTime, runningProcess->waitingTime);
                runningProcess->waitingTime = getClk() + runningProcess->remainingTime - (runningProcess->arrivalTime + runningProcess->runningTime); // ADDED
                schduelerLog(runningProcess);
            }
        }
    }
    // Nothing else to do in the current iteration, return
}
void SRTN()
{
    printf("Running SRTN Scheduler\n");
    // Receive any new processes
    receiveFromProcGen(SRTN_ALGO);
    if (runningProcess)
    {
        countActive++;
        runningProcess->remainingTime--;
        processMsg.done = true;
        processMsg.mType = runningProcess->PID;
        printf("Sending to process -> PID = %d\n", runningProcess->PID);
        send = msgsnd(msgID2, &processMsg, sizeof(struct processInfo), !IPC_NOWAIT);
        if (send == -1)
        {
            perror("Error in msgsnd");
        }
        if (!IsEmpty(receivedProcesses) && peekQueue(receivedProcesses)->remainingTime < runningProcess->remainingTime)
        {
            strcpy(runningProcess->processState, "stopped");
            schduelerLog(runningProcess);
            enqueue(receivedProcesses, *runningProcess, 2); // Put it back in the queue
            runningProcess = dequeue(receivedProcesses);
            if (runningProcess->remainingTime == runningProcess->runningTime)
            {
                int PID = forkProcess();
                runningProcess->PID = PID;
                strcpy(runningProcess->processState, "started");
                printf("At\ttime\t%d\tprocess\t%d\t%s\tarr\t%d\ttotal\t%d\tremain\t%d\twait\t%d\n", getClk(), runningProcess->id, runningProcess->processState, runningProcess->arrivalTime, runningProcess->runningTime, runningProcess->remainingTime, runningProcess->waitingTime);
                runningProcess->waitingTime = getClk() + runningProcess->remainingTime - (runningProcess->arrivalTime + runningProcess->runningTime);
                schduelerLog(runningProcess);
            }
            else
            {
                printf("At\ttime\t%d\tprocess\t%d\t%s\tarr\t%d\ttotal\t%d\tremain\t%d\twait\t%d\n", getClk(), runningProcess->id, runningProcess->processState, runningProcess->arrivalTime, runningProcess->runningTime, runningProcess->remainingTime, runningProcess->waitingTime);
                strcpy(runningProcess->processState, "resumed");
                schduelerLog(runningProcess);
            }
        }
        // Check if the running process has finished
        if (runningProcess->remainingTime == 0)
        {
            // Finish the process
            waitpid(runningProcess->PID, NULL, 0);
            printf("Process %d terminated\n", runningProcess->id);
            strcpy(runningProcess->processState, "finished");
            runningProcess->finishedTime = getClk();
            runningProcess->turnAroundTime = runningProcess->finishedTime - runningProcess->arrivalTime;
            runningProcess->weightedTurnAroundTime = (float)(runningProcess->turnAroundTime) / runningProcess->runningTime;
            printf("At\ttime\t%d\tprocess\t%d\t%s\tarr\t%d\ttotal\t%d\tremain\t%d\twait\t%d\n", getClk(), runningProcess->id, runningProcess->processState, runningProcess->arrivalTime, runningProcess->runningTime, runningProcess->remainingTime, runningProcess->waitingTime);
            schduelerLog(runningProcess); // Log the finished process
            countFinishedProcesses++;
            if (countFinishedProcesses == procressCount)
                endTime = getClk();
            printf("Count Finished Processes = %d\n", countFinishedProcesses);
            enqueue(calcQueue, *runningProcess, 2);
            memoryLog(runningProcess);
            deallocateProcess(tree, runningProcess->id);
            checkAllocation(SRTN_ALGO);
            runningProcess = dequeue(receivedProcesses);
            if (runningProcess)
            {
                if (runningProcess->remainingTime == runningProcess->runningTime)
                {
                    int PID = forkProcess();
                    runningProcess->PID = PID;
                    strcpy(runningProcess->processState, "started");
                    printf("At\ttime\t%d\tprocess\t%d\t%s\tarr\t%d\ttotal\t%d\tremain\t%d\twait\t%d\n", getClk(), runningProcess->id, runningProcess->processState, runningProcess->arrivalTime, runningProcess->runningTime, runningProcess->remainingTime, runningProcess->waitingTime);
                    runningProcess->waitingTime = getClk() + runningProcess->remainingTime - (runningProcess->arrivalTime + runningProcess->runningTime);
                    schduelerLog(runningProcess);
                }
                else
                {
                    strcpy(runningProcess->processState, "resumed");
                    printf("At\ttime\t%d\tprocess\t%d\t%s\tarr\t%d\ttotal\t%d\tremain\t%d\twait\t%d\n", getClk(), runningProcess->id, runningProcess->processState, runningProcess->arrivalTime, runningProcess->runningTime, runningProcess->remainingTime, runningProcess->waitingTime);
                    runningProcess->waitingTime = getClk() + runningProcess->remainingTime - (runningProcess->arrivalTime + runningProcess->runningTime);
                    schduelerLog(runningProcess);
                }
            }
            else
            {
                printf("NO RUNNING PROCESS\n");
            }
            return;
            // Preempt if necessary
        }
    }
    else // If no process is running, start the next process
    {
        runningProcess = dequeue(receivedProcesses);
        if (runningProcess)
        {
            // Start the process
            int PID = forkProcess();
            runningProcess->PID = PID;
            strcpy(runningProcess->processState, "started");
            printf("At\ttime\t%d\tprocess\t%d\t%s\tarr\t%d\ttotal\t%d\tremain\t%d\twait\t%d\n", getClk(), runningProcess->id, runningProcess->processState, runningProcess->arrivalTime, runningProcess->runningTime, runningProcess->remainingTime, runningProcess->waitingTime);
            runningProcess->waitingTime = getClk() + runningProcess->remainingTime - (runningProcess->arrivalTime + runningProcess->runningTime);
            schduelerLog(runningProcess);
        }
    }
}

void RR()
{
    printf("Running RR Scheduler\n");
    receiveFromProcGen(RR_ALGO);
    printf("Queue after enqueueing at Clock: %d\n", getClk());
    // printQueue(receivedProcesses);
    if (runningProcess)
    {
        countActive++;
        // getClk();
        printf("Already running process: ID = %d\n", runningProcess->id);
        // printf("Ha2olaha TENAZEL WA7DA\n");
        runningProcess->remainingTime--;
        quantum--;
        // Check if the process finished its time quantum
        if (runningProcess->remainingTime <= 0)
        {
            printf("Time quantum finished for process %d\n", runningProcess->id);
            processMsg.done = true;
            processMsg.mType = runningProcess->PID;
            printf("Sending to process -> PID = %d\n", runningProcess->PID);
            send = msgsnd(msgID2, &processMsg, sizeof(struct processInfo), !IPC_NOWAIT);

            // wait for the process to terminate

            waitpid(runningProcess->PID, NULL, 0);
            printf("Process %d terminated\n", runningProcess->id);
            strcpy(runningProcess->processState, "finished");
            runningProcess->finishedTime = getClk();
            runningProcess->turnAroundTime = runningProcess->finishedTime - runningProcess->arrivalTime;
            runningProcess->weightedTurnAroundTime = (float)(runningProcess->turnAroundTime) / runningProcess->runningTime;
            schduelerLog(runningProcess);
            enqueue(calcQueue, *runningProcess, 3);
            memoryLog(runningProcess);
            deallocateProcess(tree, runningProcess->id);
            checkAllocation(RR_ALGO);
            // Reset the running process pointer for the next iteration to dequeue a new process
            runningProcess = NULL;
            countFinishedProcesses++;
            if (countFinishedProcesses == procressCount)
                endTime = getClk();
            printf("Count Finished Processes = %d\n", countFinishedProcesses);
        }
        else if (quantum != 0)
        {
            printf("Already running process: ID = %d\n", runningProcess->id);
            printf("NA2ESLY in scheduler 2= %d\n", runningProcess->remainingTime);
            processMsg.done = true;
            processMsg.mType = runningProcess->PID;
            // print PID of the process i am sending to
            send = msgsnd(msgID2, &processMsg, sizeof(struct processInfo), !IPC_NOWAIT);
        }
        else
        {
            printf("NA2ESLY in scheduler 2= %d\n", runningProcess->remainingTime);
            processMsg.done = false;
            processMsg.mType = runningProcess->PID;
            printf("Sending to process -> PID = %d\n", runningProcess->PID);
            send = msgsnd(msgID2, &processMsg, sizeof(struct processInfo), !IPC_NOWAIT);
            strcpy(runningProcess->processState, "stopped");
            schduelerLog(runningProcess);
            enqueue(receivedProcesses, *runningProcess, 3);
            runningProcess = NULL;
        }
    }
    if (!runningProcess)
    {
        runningProcess = dequeue(receivedProcesses);
        if (runningProcess)
        {
            printf("Dequeued process: %d\n", runningProcess->id);
            quantum = tempQuantum;
            // Fork the process
            int PID = forkProcess();

            // In the scheduler process, set the PID of the running process to the forked process PID
            runningProcess->PID = PID;
            if (runningProcess->runningTime == runningProcess->remainingTime) // Checking if it the first time for the process to run or not
            {
                strcpy(runningProcess->processState, "started");
                runningProcess->waitingTime = getClk() + runningProcess->remainingTime - (runningProcess->arrivalTime + runningProcess->runningTime);
                schduelerLog(runningProcess);
            }
            else
            {
                strcpy(runningProcess->processState, "resumed");
                runningProcess->waitingTime = getClk() + runningProcess->remainingTime - (runningProcess->arrivalTime + runningProcess->runningTime);
                schduelerLog(runningProcess);
            }
        }
        // If nothing was dequeued, there is no process to run
        else
        {
            printf("NO RUNNING PROCESS\n");
        }
        return;
    }
}

void clearResources(int signum)
{
    // TODO Clears all resources in case of interruption
    msgctl(msgID, IPC_RMID, (void *)NULL);
    msgctl(msgID2, IPC_RMID, (void *)NULL);
    exit(0);
}

int main(int argc, char *argv[])
{
    signal(SIGINT, clearResources);
    // INITIATING CLOCK
    initClk();
    // Preparing the path of the process.o file
    char *processBuffer[600];
    getcwd(processBuffer, sizeof(processBuffer));
    PRO_PATH = strcat(processBuffer, "/process.out");
    // Opening & Preparing Log File
    schduelerLogFile = fopen("scheduler.log", "w");
    if (schduelerLogFile == NULL)
    {
        printf("Error opening the file.\n");
        return 1;
    }
    fprintf(schduelerLogFile, "#At\ttime\tx\tprocess\ty\tstate\tarr\tw\ttotal\tz\tremain\ty\twait\tk\n");

    // Opening the memory.log file to write in it
    memoryLogFile = fopen("memory.log", "w");
    if (memoryLogFile == NULL)
    {
        printf("Error opening Memory the file.\n");
        return 1;
    }
    fprintf(memoryLogFile, "#At\ttime\tx\tallocated\ty\tbytes\tfor\tprocess\tz\tfrom\ti\tto\tj\n");

    // MESSAGE QUEUE BET PROCESS GENERATOR & SCHDUELER
    key_t key_id;
    key_id = ftok("keyfile", 65);
    msgID = msgget(key_id, 0666 | IPC_CREAT);
    if (msgID == -1)
    {
        perror("Error in create");
        exit(-1);
    }
    // MESSAGE QUEUE BET PROCESS & SCHDUELER
    key_t key = ftok("keyfile", 70);
    msgID2 = msgget(key, 0666 | IPC_CREAT);
    if (msgID2 == -1)
    {
        perror("Error creating message queue!");
        exit(EXIT_FAILURE);
    }
    printf("Schdueler msg Queue fel scheduler ID = %d\n", msgID2);
    // SEMAPHORES Bet CLOCK & Scheduler
    int sem_sync = -1;
    key_t key_sem_sync = ftok("keyfile", 80);
    sem_sync = semget(key_sem_sync, 1, 0666);
    receivedProcesses = createPriorityQueue(); // Create Queue for received Processes
    calcQueue = createPriorityQueue();         // Create Queue for calculations
    waitingProcesses = createPriorityQueue();  // Create Queue for Processes waiting for memory allocation
    tree = createMemoryTree();                 // Create Tree presenting the memory
    int algorithmUsed = atoi(argv[1]);
    quantum = atoi(argv[2]);
    tempQuantum = quantum;
    procressCount = atoi(argv[3]);
    while (processSending)
    {
        if (algorithmUsed == 1) // 1 FOR HPF ALGORITHM
        {
            quantum = 1;
            printf("ESHTA8ALY YA CLOCK\n");
            down(sem_sync);
            HPF();
            sleep(1);
            if (countFinishedProcesses == procressCount)
            {
                printf("CONDITION MET\n");
                break;
            }
            else
            {
                printf("CONDITION NOT MET\n");
            }
        }
        else if (algorithmUsed == 2) // 2 FOR SRTN ALGORITHM
        {
            quantum = 1;
            down(sem_sync);
            SRTN();
            if (countFinishedProcesses == procressCount)
            {
                printf("CONDITION MET\n");
                break;
            }
            else
            {
                printf("CONDITION NOT MET\n");
                printf("Count Finished Processes = %d\n", countFinishedProcesses);
            }
        }
        // ELSE used as checking the input was already done in the process generator
        else // 3 FOR RR ALGORITHM
        {
            down(sem_sync);
            RR();
            sleep(1);
            if (countFinishedProcesses == procressCount)
            {
                printf("CONDITION MET\n");
                break;
            }
            else
            {
                printf("CONDITION NOT MET\n");
            }
        }
    }
    fclose(schduelerLogFile);
    free(receivedProcess);
    perfFile = fopen("scheduler.perf", "w");
    // TODO implement the scheduler :)

    // CALCULATIONS TO BE WRITTEN IN SCHEDULER.PERF
    float cpuUtil = ((float)countActive / (endTime - startTime)) * 100;
    float avgWaitTime = 0;
    float avgWTA = 0;
    float stdWTA = 0;
    int waitingTimeSum;
    float sumWTASquared = 0;
    float weightedTASum = 0;
    int processesCount = calcQueue->count;
    // Calculation of the Avg Waiting time & Avg WTA
    for (int i = 0; i < procressCount; i++)
    {
        struct processInfo *calcProcess = dequeue(calcQueue);
        calcProcess->turnAroundTime = calcProcess->finishedTime - calcProcess->arrivalTime;
        calcProcess->waitingTime = calcProcess->turnAroundTime - calcProcess->runningTime;
        calcProcess->weightedTurnAroundTime = (float)calcProcess->turnAroundTime / calcProcess->runningTime;
        waitingTimeSum += calcProcess->waitingTime;
        weightedTASum += calcProcess->weightedTurnAroundTime;
        enqueue(calcQueue, *calcProcess, 3);
    }
    avgWaitTime = (float)waitingTimeSum / procressCount;
    avgWTA = (float)weightedTASum / procressCount;

    // Calculation of the standard deviation of the WTA
    for (int i = 0; i < procressCount; i++)
    {
        struct processInfo *calcProcess = dequeue(calcQueue);
        float difference = calcProcess->weightedTurnAroundTime - avgWTA;
        sumWTASquared += (difference * difference);
    }

    float meanWTASquared = sumWTASquared / procressCount;
    stdWTA = sqrt(meanWTASquared);
    fprintf(perfFile, "CPU\tutlilization\t=\t%.2f%%\n", cpuUtil);
    fprintf(perfFile, "Avg\tWTA\t=\t%.2f\n", avgWTA);
    fprintf(perfFile, "Avg\tWaiting\t=\t%.2f\n", avgWaitTime);
    fprintf(perfFile, "Std\tWTA\t=\t%.2f", stdWTA);
    fclose(perfFile);
    fclose(memoryLogFile);
    destroyClk(true);
    clearResources(SIGINT);
    return 0;
}
