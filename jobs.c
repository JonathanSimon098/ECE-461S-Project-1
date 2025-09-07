#include "jobs.h"
#include <string.h>

Node* create_job(pid_t pid, char* command, JobState state, int newjobid) {
    // Allocate memory for new node
    Node* newNode = (Node*)malloc(sizeof(Node));
    if ( newNode == NULL ) {
        printf("Error: Memory allocation failed at linked list\n");
        exit(1);
    }
    newNode->jobid = newjobid;
    newNode->pid = pid;
    newNode->command = strdup(command);
    newNode->state = state;
    newNode->next = NULL;
    return newNode;
}

void insert_job(Node** head, pid_t pid, char* command, JobState state) {
    int newjobid;
    if (*head == NULL) {
        newjobid = 1;
    } else {
        newjobid = (*head)->jobid + 1;
    }
    Node* newNode = create_job(pid, command, state, newjobid);
    newNode->next = *head; // Points to old head
    *head = newNode; // Makes new node the HEAD (the beginning)
}

// Initial call to print_jobs needs to make most_recent_job equal to 1
int print_jobs(const Node* head, int most_recent_job) {
    if ( head == NULL ) return -1;
    if ( head->next == NULL ) return most_recent_job;

    int is_recent_job = most_recent_job;
    if ( is_recent_job && (head->state == STOPPED) ) {
        // First encounter of 'Stopped' guarantees it is at least the most recent
        is_recent_job = 0;
    }

    int another_recent_job = print_jobs(head->next, is_recent_job);

    char recent_job = '+';
    if ( !most_recent_job && another_recent_job == 0 ) {
        // If the current job is 'Done' then it is not most recent
        recent_job = '-';
    }

    char* state = "";
    switch ( head->state ) {
        case STOPPED:
            state = "Stopped";
            break;
        case RUNNING:
            state = "Running";
            break;
        case DONE:
            state = "Done";
            break;
    }
    printf("[%d]%c %s          %s\n", head->jobid, recent_job, state, head->command);
    return most_recent_job;
}

void delete_job(Node** head, Node* job_to_delete, Node* prev_job) {
    if ( (*head)->jobid == job_to_delete->jobid ) {
        // Deleting the HEAD
        *head = job_to_delete->next;
    } else if ( job_to_delete->next == NULL ) {
        // Deleting the TAIL
        prev_job->next = NULL;
    } else {
        // Deleting in between
        prev_job->next = job_to_delete->next;
    }

    free(job_to_delete->command);
    free(job_to_delete);
}

void terminate_job(Node** head, int jobid) {
    Node* current = *head;
    Node* prevNode = NULL;
    Node* nextNode;

    while ( current != NULL ) {
        nextNode = current->next;
        if ( current->jobid == jobid ) {
            delete_job(head, current, prevNode);
            break;
        }
        prevNode = current;
        current = nextNode;
    }
}

void free_jobs(Node **head) {
    Node* current = *head;
    Node* nextNode;

    while ( current != NULL ) {
        nextNode = current->next;
        free(current->command);
        free(current);
        current = nextNode;
    }
    *head = NULL;
}