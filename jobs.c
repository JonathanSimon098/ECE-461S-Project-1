#include "jobs.h"
#include <string.h>

Node* create_job(pid_t pid, pid_t pgid, char* command, JobState state, int newjobid) {
    // Allocate memory for new node
    Node* newNode = (Node*)malloc(sizeof(Node));
    if ( newNode == NULL ) {
        printf("Error: Memory allocation failed at linked list\n");
        exit(1);
    }
    newNode->jobid = newjobid;
    newNode->pid = pid;
    newNode->pgid = pgid;
    newNode->command = strdup(command);
    newNode->state = state;
    newNode->next = NULL;
    return newNode;
}

int insert_job(Node** head, pid_t pid, pid_t pgid, char* command, JobState state) {
    int newjobid;
    if (*head == NULL) {
        newjobid = 1;
    } else {
        newjobid = (*head)->jobid + 1;
    }
    Node* newNode = create_job(pid, pgid, command, state, newjobid);
    newNode->next = *head; // Points to old head
    *head = newNode; // Makes new node the HEAD (the beginning)
    return newjobid;
}

void update_job_status(Node* head, pid_t pid, JobState state) {
    Node* current = head;
    Node* nextNode;

    while ( current != NULL ) {
        nextNode = current->next;
        if ( current->pid == pid ) {
            current->state = state;
            break;
        }
        current = nextNode;
    }
}

// Initial call to print_jobs needs to make most_recent_job equal to 1
int print_jobs(Node** head, const Node* current, int most_recent_job) {
    if ( current == NULL ) return most_recent_job;

    int is_recent_job = most_recent_job;
    if ( is_recent_job) {
        // First encounter of 'Stopped' guarantees it is at least the most recent
        is_recent_job = 0;
    }

    int another_recent_job = print_jobs(head, current->next, is_recent_job);

    char recent_job = '+';
    if ( !most_recent_job && another_recent_job == 0 ) {
        // If the current job is 'Done' then it is not most recent
        recent_job = '-';
    }

    char* state = "";
    switch ( current->state ) {
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
    printf("[%d]%c %s          %s\n", current->jobid, recent_job, state, current->command);
    if (current->state == DONE) {
        terminate_job(head, current->jobid);
    }
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

Node* find_last_stopped_job(Node* head) {
    Node* current = head;
    while ( current != NULL ) {
        if ( current->state == STOPPED || current->state == RUNNING) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}