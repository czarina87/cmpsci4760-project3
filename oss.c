#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <getopt.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <string.h>

#define MAX_PROCESSES 20
#define MSGSZ 100 // Adjust the size according to your needs

struct PCB
{
    int occupied;     // 0 for false, 1 for true
    pid_t pid;        // process id of this child
    int startSeconds; // time when it was forked
    int startNano;    // time when it was forked
} processTable[MAX_PROCESSES];

struct msgbuf
{
    long mtype;
    char mtext[MSGSZ];
};

int msgQueueId; // Message queue ID

typedef struct
{
    int seconds;
    int nanoseconds;
} SimulatedClock;

SimulatedClock *sim_clock;
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

long lastLaunchTime = 0; // Stores the last launch time in milliseconds

long getCurrentTimeMillis()
{
    return (sim_clock->seconds * 1000) + (sim_clock->nanoseconds / 1000000);
}

// Function to initialize the process table
void initializeProcessTable()
{
    for (int i = 0; i < MAX_PROCESSES; i++)
    {
        processTable[i].occupied = 0;     // Initially, all entries are not in use
        processTable[i].pid = -1;         // Indicates no process ID assigned
        processTable[i].startSeconds = 0; // Simulated start time, in seconds
        processTable[i].startNano = 0;    // Simulated start time, in nanoseconds
    }
}

// Cleanup resources and exit
void cleanupAndExit(int signum)
{
    for (int i = 0; i < MAX_PROCESSES; i++)
    {
        if (processTable[i].occupied)
        {
            kill(processTable[i].pid, SIGTERM); // Send SIGTERM to each occupied process
        }
    }
    cleanupSharedClock(); // Cleanup shared memory
    // Destroy message queue
    msgctl(msgQueueId, IPC_RMID, NULL);
    exit(0);
}

void incrementSimulatedClock(int numChildren)
{
    if (numChildren > 0)
    {
        int increment = 250 / numChildren; // Calculate the increment in milliseconds
        // Assume you have a function to increment your clock here
        // incrementClockByMilliseconds(increment);
    }
}

// Setup message queue
void setupMessageQueue()
{
    key_t key = ftok("oss.c", 'b');
    if (key == -1)
    {
        perror("ftok error");
        exit(1);
    }
    msgQueueId = msgget(key, 0666 | IPC_CREAT);
    if (msgQueueId == -1)
    {
        perror("msgget error");
        exit(1);
    }
}

void cleanupMessageQueue()
{
    // Remove the message queue
    if (msgctl(msgQueueId, IPC_RMID, NULL) == -1)
    {
        perror("msgctl IPC_RMID error");
    }
}

void sendMessageToWorker(const char *message)
{
    struct msgbuf sbuf; // Correct struct name

    // Prepare the message
    sbuf.mtype = 1; // Message type should be set to a known value that workers recognize
    strncpy(sbuf.mtext, message, MSGSZ);

    // Send the message
    if (msgsnd(msgQueueId, &sbuf, strlen(sbuf.mtext) + 1, IPC_NOWAIT) < 0)
    {
        perror("msgsnd error");
        exit(1);
    }
}

// Function to get a random time interval within the specified bounds
int getRandomInterval(int upperLimit)
{
    // Ensure this is called once in your main function: srand(time(NULL));
    return rand() % upperLimit + 1; // Random time between 1 and upperLimit
}

// Function to launch a worker with dynamic arguments
void launchWorkerWithDynamicArgs(int upperTimeLimit)
{
    // Generate random time bounds for the worker
    int timeBoundS = getRandomInterval(upperTimeLimit);
    // Convert time bounds to string for passing as arguments
    char timeBoundSStr[20];
    snprintf(timeBoundSStr, sizeof(timeBoundSStr), "%d", timeBoundS);

    // Example for nanoseconds, randomizing between 0 and 999999999
    int timeBoundNS = rand() % 1000000000;
    char timeBoundNSStr[20];
    snprintf(timeBoundNSStr, sizeof(timeBoundNSStr), "%d", timeBoundNS);

    // Prepare the arguments for execvp
    char *workerArgs[] = {"./worker", timeBoundSStr, timeBoundNSStr, NULL};

    // Fork and exec the worker process
    pid_t pid = fork();
    if (pid == 0)
    {
        // Child process
        execvp(workerArgs[0], workerArgs);
        perror("execvp failed");
        exit(EXIT_FAILURE);
    }
    else if (pid > 0)
    {
        // Parent process: update process table or perform other necessary actions
    }
    else
    {
        // Fork failed
        perror("fork failed");
        exit(EXIT_FAILURE);
    }
}

void logProcessTable(FILE *fp)
{
    if (fp != NULL)
    {
        fprintf(fp, "OSS PID:%d SysClockS: %d SysclockNano: %d\n", getpid(), sim_clock->seconds, sim_clock->nanoseconds);
        fprintf(fp, "Process Table:\n");
        fprintf(fp, "Entry\tOccupied\tPID\tStartS\tStartN\n");
        for (int i = 0; i < MAX_PROCESSES; i++)
        {
            fprintf(fp, "%d\t%d\t%d\t%d\t%d\n", i, processTable[i].occupied, processTable[i].pid, processTable[i].startSeconds, processTable[i].startNano);
        }
        fflush(fp); // Ensure the log is written immediately
    }
}

// Function prototypes
void setupMessageQueue();
void incrementClock();
void checkForTerminatedChildren();

// Command line options
struct option long_options[] = {
    {"help", no_argument, 0, 'h'},
    {"proc", required_argument, 0, 'n'},
    {"simul", required_argument, 0, 's'},
    {"timelimit", required_argument, 0, 't'},
    {"interval", required_argument, 0, 'i'},
    {"logfile", required_argument, 0, 'f'},
    {0, 0, 0, 0}};

// Main oss Executable
int main(int argc, char *argv[])
{
    struct sigaction sa_termination, sa_interrupt;
    int numProc = 0;
    int simul = 0;
    int timelimit = 0;
    int interval = 0;
    char *logfile = NULL;
    long lastLogTime = 0;

    int c; /* command line options */
    while ((c = getopt_long(argc, argv, "hn:s:t:i:f:", long_options, NULL)) != -1)
    {
        switch (c)
        {
        case 'h':
            printf("Usage: %s [-h] [-n proc] [-s simul] [-t timelimit] [-i interval] [-f logfile]\n", argv[0]);
            printf("Options:\n");
            printf(" -h             Show this help message\n");
            printf(" -n proc        Number of total children to launch\n");
            printf(" -s simul       Number of children allowed to run simultaneously\n");
            printf(" -t timelimit   Time limit for children processes\n");
            printf(" -i interval    Interval in ms to launch children\n");
            printf(" -f logfile     Path to logfile\n");
            return 0;

        case 'n':
            numProc = atoi(optarg);
            break;

        case 's':
            simul = atoi(optarg);
            break;

        case 't':
            timelimit = atoi(optarg);
            break;

        case 'i':
            interval = atoi(optarg);
            break;

        case 'f':
            logfile = strdup(optarg);
            break;

        case ':': /* Missing option argument */
            fprintf(stderr, "Option '-%c' requires an argument\n", optopt);
            return 1;

        case '?':
        default:
            fprintf(stderr, "Option '-%c' is invalid: ignored\n", optopt);
            return 1;
        }
    }

    // Setup signal handlers
    signal(SIGALRM, cleanupAndExit);
    signal(SIGINT, cleanupAndExit);
    alarm(60); // Setup a 60-second real-life timeout

    initializeProcessTable(); // Initialize the process table
    setupMessageQueue();      // Setup IPC message queue
    setupSharedClock();       // Setup shared clock
    sim_clock->seconds = 2;       // Initialize the clock
    sim_clock->nanoseconds = 50;  // Initialize the clock

    int upperTimeLimit = timelimit; // Assuming -t parameter is 7
    // launchWorkerWithDynamicArgs(upperTimeLimit);

    int activeChildren = 0; // Number of currently active children

    FILE *fp = NULL;
    if (logfile != NULL)
    {
        fp = fopen(logfile, "w");
        if (fp == NULL)
        {
            // Log to stderr if file opening fails
            fprintf(stderr, "Error opening logfile\n");
            return 1;
        }
    }
    else
    {
        fprintf(stderr, "No logfile specified\n");
        return 1;
    }

    int createdProcesses = 0; // Track the number of created processes

    while (createdProcesses < numProc || activeChildren > 0)
    {
        long currentTime = getCurrentTimeMillis();
        if (activeChildren < simul && createdProcesses < numProc && (currentTime - lastLaunchTime >= interval))
        {
            for (int i = 0; i < MAX_PROCESSES; i++)
            {
                if (!processTable[i].occupied)
                {
                    launchWorkerWithDynamicArgs(upperTimeLimit);
                    createdProcesses++;
                    activeChildren++;
                    lastLaunchTime = currentTime; // Update the last launch time
                    break;                        // Exit the for-loop after launching one process
                }
            }
        }

        // process table display
        printf("OSS PID:%d SysClockS: %d SysclockNano: %d\n", getpid(), 0, 0);
        printf("Process Table:\n");
        printf("Entry\tOccupied\tPID\tStartS\tStartN\n");
        for (int i = 0; i < MAX_PROCESSES; i++)
        {
            printf("%d\t%d\t%d\t%ld\t%ld\n", i, processTable[i].occupied, processTable[i].pid, (long int)processTable[i].startSeconds, (long int)processTable[i].startNano);
        }

        // Increment simulated clock based on active children
        incrementSimulatedClock(activeChildren);

        if (currentTime - lastLogTime >= 500)
        {                              // More than half a second has passed
            logProcessTable(fp);       
            lastLogTime = currentTime; 
        }

        // Non-blocking wait to check for any child process termination
        int status;
        pid_t pid;
        while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
        {
            for (int i = 0; i < MAX_PROCESSES; i++)
            {
                if (processTable[i].pid == pid)
                {
                    processTable[i].occupied = 0;
                    activeChildren--;
                    break;
                }
            }
        }

        
        // Minor delay to prevent this loop from consuming too much CPU
        usleep(10000); 
    }

    // Cleanup and termination logic
    cleanupAndExit(0); // Directly invoke the cleanup handler
    // close the file
    if (fp != NULL)
    {
        fclose(fp); // Close the log file when done
    }

    return 0;
}