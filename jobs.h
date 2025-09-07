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

// Define structure for a single node in the linked list
typedef struct Node {
    int jobid;
    pid_t pid;
    char* command;
    JobState state; // 0 for running, 1 for stopped, 2 for done
    struct Node* next;
} Node;

// Creates a new node with the given pid
Node* create_job(pid_t pid, char* command, JobState state, int newjobid);

// Inserts a new job with the lowest available job id
void insert_job(Node** head, pid_t pid, char* command, JobState state);

// Prints all the elements of the list from most recent to oldest (recursion)
int print_jobs(const Node* head, int most_recent_job);

// Frees memory and reassign pointers
void delete_job(Node** head, Node* job_to_delete, Node* prev_job);

// Finds jobid to terminate
void terminate_job(Node** head, int jobid);

// Frees all jobs available
void free_jobs(Node** head);

#endif //ECE_461S_PROJECT_1_PIDS_H