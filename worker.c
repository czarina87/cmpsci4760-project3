#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>

typedef struct
{
    int seconds;
    int nanoseconds;
} SimulatedClock;

SimulatedClock* sim_clock;
int shmid;

void setupSharedClock()
{
    key_t key = ftok("shmfile", 65);
    shmid = shmget(key, sizeof(SimulatedClock), 0666 | IPC_CREAT);
    sim_clock = (SimulatedClock *)shmat(shmid, (void *)0, 0);
}

void cleanupSharedClock()
{
    // Detach from shared memory
    shmdt(sim_clock);
    // Destroy the shared memory
    shmctl(shmid, IPC_RMID, NULL);
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        fprintf(stderr, "Usage: %s terminateTimeS terminateTimeNano\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int terminateTimeS = atoi(argv[1]);
    int terminateTimeNano = atoi(argv[2]);

    setupSharedClock();

    printf("WORKER PID:%d Starting\n", getpid());
    printf("System Clock: %d:%d\n", sim_clock->seconds, sim_clock->nanoseconds);
    int iterations = 0;
    do
    {
        sleep(1000); // Simulate work by sleeping for 1 second
        printf("WORKER PID:%d Iteration %d\n", getpid(), iterations);
    } while (++iterations < terminateTimeS); // Simple termination condition for demonstration

    printf("WORKER PID:%d Exiting after %d iterations\n", getpid(), iterations);

    cleanupSharedClock();
    return 0;
}
