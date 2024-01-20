#include "headers.h"
#include <time.h>
#include <stdbool.h>

#define MAX_BUFFER_SIZE 128

int remainingTime;
int msgID2;
int send;
int algorithmUsed;
int quantum;
struct msgProcess msg;
struct msgProcess processMsg;

void sendFromProcessToSchedular(int algorithmUsed, int quantum, int remainingTime)
{

    msg.mType = 2; // Message type
    msg.done = false;

    if (remainingTime == 0)
    {
        msg.done = true;
    }

    send = msgsnd(msgID2, &msg, sizeof(msg) - sizeof(long), !IPC_NOWAIT);
    if (send == -1)
    {
        perror("Error sending message to the scheduler!");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[])
{
    initClk();
    printf("STARTED RUNNING A PROCESS FILE\n");
    key_t key = ftok("keyfile", 70);
    msgID2 = msgget(key, 0666);
    if (msgID2 == -1)
    {
        perror("Error creating message queue!");
        exit(EXIT_FAILURE);
    }

    // Algorithm and quantum values should be initialized or passed as arguments
    remainingTime = atoi(argv[1]);
    quantum = atoi(argv[2]);

    while (1)
    {
        // Print pid of the process
        printf("In process -> PID = %d\n", getpid());
        // Receive message from scheduler
        msgrcv(msgID2, &processMsg, sizeof(struct processInfo), getpid(), !IPC_NOWAIT);
        // print the message received
        printf("Message received from scheduler: %d\n", processMsg.mType);
        remainingTime--;
        if (remainingTime == 0)
            break;
    }
    return 0;
}