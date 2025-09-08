//
// Created by simowjon on 9/6/25.
//

#ifndef ECE_461S_PROJECT_1_PIDS_H
#define ECE_461S_PROJECT_1_PIDS_H

#include <stdio.h>
#include <stdlib.h>

typedef enum {
    STOPPED,
    RUNNING,
    DONE
} JobState;

// Represents a single command
typedef struct Command {
    char** argv;    // Array of arguments to send to execvp
    int argCount;   // Argument count including NULL
    int argSize;    // For heap resizing
    char* inputFile;
    char* outputFile;
    char* errorFile;
    struct Command* next; // Pointer to the next command
} Command;

// Define structure for a single node in the linked list
typedef struct Job {
    Command* commands;
    int jobid;
    pid_t pgid;
    char* full_command_line;
    JobState state; // 0 for running, 1 for stopped, 2 for done
    int is_background;
    struct Job* next;
} Job;

// Inserts a new job with the lowest available job id
int insert_job(Job** head, Job* newJob);

// Sets the job status of job liked to pid
void update_job_status(Job* head, pid_t pid, JobState state);

// Prints all the elements of the list from most recent to oldest
int print_jobs(Job** head);

// Frees memory and reassign pointers
void delete_job(Job** head, Job* job_to_delete, Job* prev_job);

// Finds jobid to terminate
void terminate_job(Job** head, int jobid);

// Frees all jobs available
void free_jobs(Job** head);

// Finds the job with the highest ID that is stopped
Job* find_recent_job(Job* head);

#endif //ECE_461S_PROJECT_1_PIDS_H