#include "headers.h"

#define MAX_BUFFER_SIZE 128
int msgID;
int clkTime = 0;
int currentTime = 0;

void clearResources(int);

int countProcess(char *file)
{
    int count = 0;

    FILE *inputFile = fopen(file, "r");
    if (!inputFile)
    {
        perror("Error opening file!");
        exit(EXIT_FAILURE);
    }

    char buffer[MAX_BUFFER_SIZE];
    while (fgets(buffer, sizeof(buffer), inputFile))
    {
        count++;
    }
    count--; // So that I can ignore the first line

    fclose(inputFile);
    return count;
}

int main(int argc, char *argv[])
{
    signal(SIGINT, clearResources);
    // TODO Initialization
    // 1. Read the input files.
    char *filename = "processes.txt";
    int processCount = countProcess(filename);

    struct processInputData processTable[processCount];
    struct processInfo processes[processCount];

    FILE *inputFile = fopen(filename, "r");
    if (!inputFile)
    {
        perror("Error opening file!");
        exit(EXIT_FAILURE);
    }

    // ROUTING FOR THE EXECL OPERATIONS
    char clkBuffer[600];
    char schedularBuffer[600];
    getcwd(clkBuffer, sizeof(clkBuffer));
    getcwd(schedularBuffer, sizeof(schedularBuffer));
    char *CLK_PATH = strcat(clkBuffer, "/clk.out");
    char *SCHEDULAR_PATH = strcat(schedularBuffer, "/scheduler.out");

    char buffer[MAX_BUFFER_SIZE];
    fgets(buffer, sizeof(buffer), inputFile); // Ignore first line in the text file
    for (int i = 0; i < processCount; i++)
    {
        fscanf(inputFile, "%d\t%d\t%d\t%d\t%d", &processTable[i].id, &processTable[i].arrivalTime, &processTable[i].runningTime, &processTable[i].priority, &processTable[i].memSize);
    }
    fclose(inputFile);
    // 2. Ask the user for the chosen scheduling algorithm and its parameters, if there are any.
    // getUserInput();
    printf("Choose a scheduling algorithm:\n");
    printf("1. Non-preemptive Highest Priority First (HPF)\n");
    printf("2. Shortest Remaining time Next (SRTN).\n");
    printf("3. Round Robin (RR)\n");

    int choice;
    int quantum;
    printf("Enter the corresponding number: ");
    scanf("%d", &choice);

    switch (choice)
    {
    case 1:
        printf("Selected scheduling algorithm: HPF\n");
        break;
    case 2:
        printf("Selected scheduling algorithm: SRTN\n");
        break;
    case 3:
        printf("Selected scheduling algorithm: RR\n");
        do
        {
            printf("Enter Quantum of the RR: ");
            scanf("%d", &quantum);
            if (quantum < 1)
            {
                printf("Invalid input. Please enter a value greater than or equal to 1.\n");
            }
        } while (quantum < 1);
        break;
    default:
        printf("Invalid choice. Exiting...\n");
        exit(EXIT_FAILURE);
    }
    // 3. Initiate and create the scheduler and clock.

    char choice_str[12];
    char quantum_str[12];
    char processCounter[12];

    sprintf(choice_str, "%d", choice);
    sprintf(quantum_str, "%d", quantum);
    printf("Process Count %d\n", processCount);
    sprintf(processCounter, "%d", processCount);
    printf("Process Counter %s\n", processCounter);

    // Fork to create the clock process
    pid_t schedulerPid;
    pid_t clockPid = fork();

    if (clockPid == -1)
    {
        perror("Error forking clock process!");
        exit(EXIT_FAILURE);
    }
    else if (clockPid == 0)
    {
        // This is the child process (clock)
        execl(CLK_PATH, "clk.out", NULL);
        perror("Error executing clock!");
        exit(EXIT_FAILURE);
    }
    else
    {
        schedulerPid = fork();
        if (schedulerPid == -1)
        {
            perror("Error forking scheduler process!");
            exit(EXIT_FAILURE);
        }
        else if (schedulerPid == 0)
        {
            // This is the child process (scheduler)
            execl(SCHEDULAR_PATH, "scheduler.out", choice_str, quantum_str, processCounter, NULL);
            perror("Error executing scheduler!");
            exit(EXIT_FAILURE);
        }
    }
    // 4. Use this function after creating the clock process to initialize clock
    initClk();
    // To get time use this
    clkTime = getClk();
    printf("Current time is %d\n", clkTime);
    // TODO Generation Main Loop
    // 5. Create a data structure for processes and provide it with its parameters.
    for (int i = 0; i < processCount; i++)
    {
        processes[i].id = processTable[i].id;
        processes[i].arrivalTime = processTable[i].arrivalTime;
        processes[i].runningTime = processTable[i].runningTime;
        processes[i].priority = processTable[i].priority;
        processes[i].memSize = processTable[i].memSize;
        processes[i].PID = 0;
        strcpy(processes[i].processState, "ready");
        processes[i].remainingTime = processes[i].runningTime;
        processes[i].finishedTime = 0;
        processes[i].waitingTime = 0;
        processes[i].turnAroundTime = 0;
        processes[i].weightedTurnAroundTime = 0.0;
    }
    printf("Entering\n");
    // 6. Send the information to the scheduler at the appropriate time.
    // Create a message queue (you need to handle this based on your specific system)
    key_t key = ftok("keyfile", 65);
    msgID = msgget(key, 0666 | IPC_CREAT);
    if (msgID == -1)
    {
        perror("Error creating message queue!");
        exit(EXIT_FAILURE);
    }
    printf("Process Generator Message Queue ID = %d\n", msgID);

    // Send process information to the scheduler
    struct msgbuff msg;
    msg.mType = 1; // Message type
    int send, sendAction;
    int sentProcess = 0;
    printf("Process Count %d", processCount);
    while (sentProcess < processCount)
    {
        if (currentTime <= getClk())
        { // If the process generator is late and should execute it's code
            while (processes[sentProcess].arrivalTime == currentTime)
            {
                msg.mprocess = processes[sentProcess];
                printf("Ha START SENDING\n");
                send = msgsnd(msgID, &msg, sizeof(struct processInfo), !IPC_NOWAIT);
                // printSingleProcess(processes);
                printf("ProcessID: %d\n", processes[sentProcess].id);
                if (send == -1)
                {
                    perror("Error sending message to the scheduler!");
                    exit(EXIT_FAILURE);
                }
                sentProcess++;
            }
            currentTime++;
        }
    }

    printf("Process information sent to the scheduler successfully.\n");
    // 7. Clear clock resources
    int pStatus;
    waitpid(schedulerPid, &pStatus, 0);
    if (WIFEXITED(pStatus))
        clearResources(SIGINT);
}

void clearResources(int signum)
{
    // TODO Clears all resources in case of interruption
    msgctl(msgID, IPC_RMID, (void *)NULL);
    destroyClk(true);
    exit(0);
}