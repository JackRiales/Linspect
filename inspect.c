/*
=====================================================================================
    Linux Inspect
    Jack Riales, CSC 322-501 Spring

    Output the following information:
    -- Linux version
    -- Amount of time that the CPU has spent in "user mode," "system mode," and "idle"
    -- Number of context switches performed by the kernel
    -- Number of processes created since system was booted
    -- The amount of memory currently in use
    -- The total amount of memory available in the system (including swap space)
=====================================================================================
*/

// Checking os compatability.
// Only linux systems will be able to run this successfully.
#ifdef __linux__
    #define OSCOMPAT 1
#else
    #define OSCOMPAT 0
#endif // __unix__

// Exit return values
#define EXIT_SUCCESS 0
#define EXIT_FAILURE -1

// Includes
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

// Determines whether or not to print debug information
static bool verbose;

// Determines whether or not to use swap space in memory info
static bool useSwap;

// Struct to contain cpu times later
typedef struct {
    unsigned    user,
                nice,
                system,
                idle,
                iowait,
                irq,
                softirq;
} cpu_time;

/*
=====================================================================================
    Displays help info. Used in main when the argument is -h or --help
=====================================================================================
*/
int help() {
    char const* help = "\nLinInspect : Written by Jack Riales\n\nVerbosity\t-v\tEnables printing of debug information.\nSwap Space\t-s\tIncludes any available swap space in\n\t\t\tshowing the memory information.\nHelp\t\t-h\tPrints this help information.";
    printf("%s\n\n", help);
    return 0;
}

/*
=====================================================================================
    Uses uname to get and print the linux version
=====================================================================================
*/
int printLinuxVersion() {
    struct utsname uname_bf;
    if (uname(&uname_bf) < 0) {
        perror("Could not perform uname. Exiting.");
        return -1;
    }
    printf ("\x1b[36mLinux Version: %s\x1b[0m\n", uname_bf.release);
    return 0;
}

/*
=====================================================================================
    Parses /proc/stat (file) and returns the number of processes that have been created since boot
=====================================================================================
*/
long processesSinceBoot() {
    // Try to open proc stat for reading
    FILE *stat = fopen("/proc/stat", "r");

    // Ensure we have it open
    if (stat == NULL) {
        perror("Unable to open /proc/stat.");
        return -1l;
    }

    // It's open, so create a buffer to start reading lines
    char ch_buf[256];

    // Read each line
    while (fgets(ch_buf, sizeof ch_buf, stat) != NULL) {
        // Look for the word "processes" in the line
        if (strstr(ch_buf, "processes") != NULL) {
            char label [32];
            int proc_count;

            // Scan the line and apply the tokens
            sscanf (ch_buf, "%s %d", label, &proc_count);

            // Return the process count
            return proc_count;
        }
    }

    perror("Unable to parse /proc/stat. No process count was found.");
    return 0l;
}

/*
=====================================================================================
    Parses /proc/stat (file) and returns the number of processes that have been created since boot
=====================================================================================
*/
long contextSwitchesSinceBoot() {
    // Try to open proc stat for reading
    FILE *stat = fopen("/proc/stat", "r");

    // Ensure we have it open
    if (stat == NULL) {
        perror("Unable to open /proc/stat.");
        return -1l;
    }

    // It's open, so create a buffer to start reading lines
    char ch_buf[256];

    // Read each line
    while (fgets(ch_buf, sizeof ch_buf, stat) != NULL) {
        // Look for the word "processes" in the line
        if (strstr(ch_buf, "ctxt") != NULL) {
            char label [32];
            int switch_count;

            // Scan the line and apply the tokens
            sscanf (ch_buf, "%s %d", label, &switch_count);

            // Return the process count
            return switch_count;
        }
    }

    perror("Unable to parse /proc/stat. No process count was found.");
    return 0l;
}

/*
=====================================================================================
    Returns the amount of virtual memory that is being used up
    \param a pointer to the struct to assign the values to
    \return if /proc/stat was
=====================================================================================
*/
bool getCpuTimes(cpu_time *cpu) {
    // Open proc stat for reading
    FILE *stat = fopen("/proc/stat", "r");

    // Ensure it's open
    if (stat == NULL) {
        perror("Unable to open /proc/stat.");
        return false;
    }

    // It's open, so create a buffer
    char ch_buf[256];

    // Get the first line
    fgets(ch_buf, sizeof ch_buf, stat);

    // Set up a temporary struct to contain the variables of the line
    char label[3];
    cpu_time tmpTimes;

    // Scan and place in the variables
    sscanf(ch_buf, "%s %u %u %u %u %u %u %u",
           label,
           &tmpTimes.user, &tmpTimes.nice, &tmpTimes.system, &tmpTimes.idle,
           &tmpTimes.iowait, &tmpTimes.irq, &tmpTimes.softirq);

    // Give to the parameter
    cpu->user = tmpTimes.user;
    cpu->nice = tmpTimes.nice;
    cpu->system = tmpTimes.system;
    cpu->idle = tmpTimes.idle;
    cpu->iowait = tmpTimes.iowait;
    cpu->irq = tmpTimes.irq;
    cpu->softirq = tmpTimes.softirq;

    return true;
}

/*
=====================================================================================
    Returns the amount of virtual memory that is being used up
    \param withSwap Include any swap space in the system along with the RAM
    \return used memory, in bytes
=====================================================================================
*/
long long usedVirtualMemory(bool withSwap) {
    // Create a structure for system info
    struct sysinfo mem;

    // Take in the system info
    sysinfo(&mem);

    // Figure out the memory used in bytes
    long long usedMem = mem.totalram - mem.freeram;

    // If the user wants to account for swap space, do that as well.
    if (withSwap) {
        usedMem += mem.totalswap - mem.freeswap;
    }
    if (verbose) {
        printf ("Used : %lld\n", usedMem);
        printf ("Mem Unit : %d\n", mem.mem_unit);
    }

    // Ensure that it doesn't overflow
    usedMem *= mem.mem_unit;

    return usedMem;
}

/*
=====================================================================================
    Returns the total virtual memory available
    \param withSwap Include any swap space in the system along with the RAM
    \return total memory, in bytes
=====================================================================================
*/
long long totalVirtualMemory(bool withSwap) {
    // Create a structure for system info
    struct sysinfo mem;

    // Take in the system info
    sysinfo(&mem);

    if (withSwap)
        return mem.totalram + mem.totalswap;
    else
        return mem.totalram;
}

/*
=====================================================================================
    Checks /proc and returns directory info
=====================================================================================
*/
int proc_ls(char** accept, bool verbose) {
    // Open the /proc directory
    DIR* dir = opendir("/proc");

    // Create a structure for reading the information
    struct dirent *ent;

    // Ensure the directory info is not null
    if (dir != NULL) {
        // Since we're using c++, let's use STL to store some char arrays
        int ls_buf_len = 256;
        char* ls[ls_buf_len];
        int ls_i = 0;

        // Run through and read the information, sort of like 'ls'
        while ((ent = readdir(dir)) != NULL) {
            if (ls_i < ls_buf_len) {
                // Add each item to the vector
                ls[ls_i] = (ent->d_name);

                // If verbose, print it out as well.
                if (verbose) {
                    printf("Received %s\n.", ent->d_name);
                }
            }
        }

        // Close the stream
        closedir(dir);

        // Set the value
        accept = ls;

        // Exit
        return 0;
    } else {
        perror("Unable to open '/proc' directory.");
        return -1;
    }
}

/*
=====================================================================================
    Main driver. Checks OS compatability and runs method calls.
=====================================================================================
*/
int main(int argc, char** argv) {
    if (OSCOMPAT) {
        if (argc > 1) {
            for (int i = 1; i < argc; ++i) {
                // Gather the argument provided
                char * arg = argv[i];

                // Use the help method
                if (!strcmp(arg, "-h") || !strcmp(arg, "--help"))
                    return help();

                // Enable verbosity mode
                if (!verbose)
                    verbose = (!strcmp(arg, "-v"));

                // Enable swap space usage
                if (!useSwap)
                    useSwap = (!strcmp(arg, "-s"));
            }
        }

        // First thing, use uname to get the linux distro and version
        printLinuxVersion();

        // Get CPU time information
        cpu_time *cpu = (cpu_time *) malloc(sizeof(cpu_time));
        if (!getCpuTimes(cpu)) {
            perror("Could not get the cpu times!\n");
            return EXIT_FAILURE;
        } else {
            printf("CPU Times ================\n\tUser:\t%u\n\tKernel:\t%u\n\tIdle:\t%u\n", cpu->user, cpu->system, cpu->idle);
        }
        free(cpu);

        // Print the context switches since boot time.
        printf("Context Switches: %ld\n", contextSwitchesSinceBoot());

        // Print the processes started since boot time.
        printf("Processes Since Boot: %ld\n", processesSinceBoot());

        // Print the amount of used memory
        if (useSwap)
            printf("Memory used: %lld bytes out of %lld (including swap space)\n", usedVirtualMemory(true), totalVirtualMemory(true));
        else
            printf("Memory used: %lld bytes out of %lld\n", usedVirtualMemory(false), totalVirtualMemory(false));
    } else {
        perror("Platform not supported. Please use a linux based OS to run this.");
        return EXIT_FAILURE;
    }
}
