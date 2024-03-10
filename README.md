https://github.com/czarina87/cmpsci4760-project3

OSS and Worker Application
Overview
This application simulates an operating system scheduler (OSS) managing worker processes. The OSS executable launches worker processes at specified intervals, controls their execution time, and logs their activity. The system uses inter-process communication (IPC) through message queues, allowing OSS and workers to exchange messages.

Features
Dynamic Process Management: Launches and manages a specified number of worker processes.
Simulated Clock: Implements a simulated system clock that increments based on the number of active children.
Inter-Process Communication: Utilizes message queues for communication between the OSS and worker processes.
Signal Handling: Gracefully handles termination signals (SIGINT) and timeout (SIGALRM) to ensure proper cleanup.
Logging: Logs all OSS activities to both the console and a specified log file.
Installation
Clone the repository to your local machine:

bash
Copy code
git clone https://github.com/your-repository/oss-worker-app.git
cd oss-worker-app
Compile the application using gcc:

bash
Copy code
gcc -o oss oss.c -lrt
gcc -o worker worker.c -lrt
Note: -lrt is used for linking the real-time library, necessary for message queue operations.

Usage
The application is invoked with the oss executable, followed by command-line options to customize its behavior:

bash
Copy code
./oss [-h] [-n proc] [-s simul] [-t timelimitForChildren] [-i intervalInMsToLaunchChildren] [-f logfile]
Command-Line Options
-h: Displays help information.
-n proc: Specifies the total number of child processes to launch.
-s simul: Sets the maximum number of child processes that can run simultaneously.
-t timelimitForChildren: Defines the upper bound (in seconds) for the random execution time assigned to each child process.
-i intervalInMsToLaunchChildren: Indicates the minimum interval (in milliseconds) between launching child processes.
-f logfile: Specifies the filename for logging OSS activities.
Example
To run the application with 5 total processes, allowing 3 to run simultaneously, with child processes having a maximum execution time of 7 seconds, launching children every 100 milliseconds, and logging to oss.log:

bash
Copy code
./oss -n 5 -s 3 -t 7 -i 100 -f oss.log
Cleanup
The application handles SIGINT (Ctrl-C) and SIGALRM signals for graceful termination, ensuring all resources are released properly. If the application exits unexpectedly, you may need to remove the message queue manually using ipcrm.