#include "jobs.h"
#include <string.h>

int insert_job(Job** head, Job* newJob) {
    int newjobid;
    if (*head == NULL) {
        newjobid = 1;
    } else {
        newjobid = (*head)->jobid + 1;
    }
    //Job* newJob = create_job(pid, pgid, command, state, newjobid);
    if (newJob == NULL) return -1;
    newJob->jobid = newjobid;
    newJob->next = *head; // Points to old head
    *head = newJob; // Makes new node the HEAD (the beginning)
    return newjobid;
}

void update_job_status(Job* head, pid_t pgid, JobState state) {
    Job* current = head;
    Job* nextNode;

    while ( current != NULL ) {
        nextNode = current->next;
        if ( current->pgid == pgid ) {
            current->state = state;
            break;
        }
        current = nextNode;
    }
}

// Initial call to print_jobs needs to make most_recent_job equal to 1
int print_jobs(Job** head) {

    char** job_outputs = (char**) malloc(sizeof(char*) * 20); // Max jobs is 20
    if (job_outputs== NULL) {
        perror("print_jobs");
        return -1;
    }
    int job_output_size = 0;

    Job* current = *head;
    Job* nextNode;
    char recent_job = '+';

    while ( current != NULL) {
        if (job_output_size > 19) break;
        nextNode = current->next;

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

        const size_t buffer_size = strlen(state) + 17 + strlen(current->full_command_line); // null terminator included
        char* job_out = (char*)malloc(buffer_size);
        snprintf(job_out, buffer_size, "[%d]%c %s          %s\n", current->jobid, recent_job, state, current->full_command_line);
        job_outputs[job_output_size++] = job_out;
        if (current->state == DONE) {
            terminate_job(head, current->jobid);
        }
        recent_job = '-';
        current = nextNode;
    }

    for (int i = job_output_size - 1; i >= 0; i--) {
        printf("%s", job_outputs[i]);
        free(job_outputs[i]);
    }

    free(job_outputs);
    return 0;
}

void delete_job(Job** head, Job* job_to_delete, Job* prev_job) {
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

    Command* current = job_to_delete->commands;
    Command* nextNode;

    while ( current != NULL ) {
        nextNode = current->next;
        for (int i = 0; i < current->argCount; i++) free(current->argv[i]);
        free(current->argv);
        if (current->inputFile) free(current->inputFile);
        if (current->outputFile) free(current->outputFile);
        if (current->errorFile) free(current->errorFile);
        free(current);
        current = nextNode;
    }

    free(job_to_delete->full_command_line);
    free(job_to_delete);
}

void terminate_job(Job** head, int jobid) {
    Job* current = *head;
    Job* prevNode = NULL;
    Job* nextNode;

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

void free_jobs(Job **head) {
    Job* current = *head;
    Job* nextNode;

    while ( current != NULL ) {
        nextNode = current->next;
        free(current->full_command_line);
        free(current);
        current = nextNode;
    }
    *head = NULL;
}

Job* find_recent_job(Job* head, const int fg) {
    Job* current = head;
    while ( current != NULL ) {
        if ( current->state == STOPPED || (current->state == RUNNING && fg)) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}