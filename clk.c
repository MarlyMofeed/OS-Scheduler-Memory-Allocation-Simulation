/*
 * This file is done for you.
 * Probably you will not need to change anything.
 * This file represents an emulated clock for simulation purpose only.
 * It is not a real part of operating system!
 */

#include "headers.h"

int shmid;
union Semun semun;
int sem_sync = -1;

/* Clear the resources before exit */
void cleanup(int signum)
{
    shmctl(shmid, IPC_RMID, NULL);
    semctl(sem_sync, 0, IPC_RMID, semun);
    printf("Clock terminating!\n");
    exit(0);
}
int up(int sem)
{
    int check = semctl(sem_sync, 0, GETVAL, semun);
    if (check == 1)
        return 0;
    struct sembuf op;

    op.sem_num = 0;
    op.sem_op = 1;
    op.sem_flg = !IPC_NOWAIT;

    if (semop(sem, &op, 1) == -1)
    {
        perror("Error in up()");
        exit(-1);
    }
    return 1;
}

/* This file represents the system clock for ease of calculations */
int main(int argc, char *argv[])
{
    printf("Clock starting\n");
    signal(SIGINT, cleanup);
    int clk = 0;
    // Create shared memory for one integer variable 4 bytes
    shmid = shmget(SHKEY, 4, IPC_CREAT | 0644);
    if ((long)shmid == -1)
    {
        perror("Error in creating shm!");
        exit(-1);
    }
    int *shmaddr = (int *)shmat(shmid, (void *)0, 0);
    if ((long)shmaddr == -1)
    {
        perror("Error in attaching the shm in clock!");
        exit(-1);
    }
    *shmaddr = clk; /* initialize shared memory */

    key_t key_sem_sync = ftok("keyfile", 80);
    sem_sync = semget(key_sem_sync, 1, 0666 | IPC_CREAT);

    if (sem_sync == -1)
    {
        perror("Error in create sem");
        exit(-1);
    }
    semun.val = 0;
    if (semctl(sem_sync, 0, SETVAL, semun) == -1)
    {
        perror("Error in semctl");
        exit(-1);
    }
    int check = 1;
    while (1)
    {
        sleep(1);
        // printf("ANA EL SEMAPHTORE   %d\n",sem_sync);
        printf("ANA MESTANY EL SCHDULER Y5ALAS\n");
        if (check)
        {
            (*shmaddr)++;
            printf("CLOCK: %d \n", *shmaddr);
        }
        check = up(sem_sync);
        // up feeh makanen ma hona -> elly howa gowa el while loop
    }
}
