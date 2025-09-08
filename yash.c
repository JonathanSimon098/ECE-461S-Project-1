//
// ECE 461S Fall 2025
// Section 18850
// Jonathan Simon
// jes7539
//

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <unistd.h>
#include "jobs.h"

Job* job_head = NULL;
volatile pid_t g_foreground_pgid = 0; // Tracks the current foreground job's PGID

int isCommand( char* tkn ) {
    if (
        strcmp(tkn, "fg") == 0 ||
        strcmp(tkn, "bg") == 0 ||
        strcmp(tkn, "jobs") == 0
        ) {
        return 1;
    }
    char* path_env = getenv("PATH");
    if (path_env == NULL) {
        // PATH NOT FOUND
        return -1;
    }
    char* path_copy = strdup(path_env);
    if (path_copy == NULL ) {
        return -1;
    }
    char* savePtr;
    char* directory;
    directory = strtok_r(path_copy, ":", &savePtr);
    while ( directory != NULL) {
        const int sizeOfDirectory = (int)(strlen(directory) + strlen(tkn) + 2); // +2 for '/' and \0
        char cmdDirectory[sizeOfDirectory];
        snprintf(cmdDirectory, sizeOfDirectory, "%s/%s", directory, tkn);

        if (access(cmdDirectory, X_OK) == 0) {
            free(path_copy);
            return 1;
        }

        directory = strtok_r(NULL, ":", &savePtr);
    }
    free(path_copy);
    return 0;
}

int isFileRedirector( char* tkn ) {
    if (
        strcmp(tkn, "<") == 0 ||
        strcmp(tkn, ">") == 0 ||
        strcmp(tkn, "2>") == 0
        ) {
        return 1;
    }
    return 0;
}

void cleanup(Command* head, char* usrInputCopy) {
    Command* current = head;
    Command* nextNode;

    while ( current != NULL ) {
        nextNode = current->next;
        for (int i = 0; i < current->argCount; i++) free(current->argv[i]);
        free(current->argv);
        if (current->inputFile) free(current->inputFile);
        if (current->outputFile) free(current->outputFile);
        if (current->errorFile) free(current->errorFile);
        current = nextNode;
    }
    free(usrInputCopy);
}

void handle_sigtstp(int sig) {
    // Handles ^C signal
    // Must quit the foreground process
    write(STDOUT_FILENO, "\n# ", 4);
}

void handle_sigint(int sig) {
    // Handles ^C signal
    // Must quit the foreground process
    write(STDOUT_FILENO, "\n# ", 4);

    if (g_foreground_pgid != 0) {
        // send SIGINT to its entire process group
        if (kill(-g_foreground_pgid, SIGINT) < 0) {
            fprintf(stderr, "yash: %s\n", strerror(errno));
            exit(1);
        }
    }
}

void handle_sigchld(int sig) {
    // Default: ignored
    // signal from child to parent when child process terminates
    pid_t pid;
    int status;

    while ( (pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0 ) {
        if (WIFSTOPPED(status)) {
            update_job_status(job_head, pid, STOPPED);
        } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
            update_job_status(job_head, pid, DONE);
        } else if (WIFCONTINUED(status)) {
            update_job_status(job_head, pid, RUNNING);
        }
    }

}

Job* parse_line(char* usrInput) {
    char* usrInputCopy = strdup(usrInput);
    if (usrInputCopy == NULL) {
        fprintf(stderr, "yash: %s\n", strerror(errno));
        return NULL;
    } // failed to allocate. exit.
    char** cmdArgs = calloc(10, sizeof(char*));
    if (cmdArgs == NULL) {
        fprintf(stderr, "yash: %s\n", strerror(errno));
        free(usrInputCopy);
        return NULL;
    } // failed to allocate. exit.

    char* saved_token_ptr;
    char* token = strtok_r(usrInputCopy, " ", &saved_token_ptr);

    Command* head = NULL;
    Job* job = NULL;
    if (token != NULL) {
        job = (Job*)malloc(sizeof(Job));
        //int job_insert = insert_job(&job, )
        if (job == NULL) {
            fprintf(stderr, "yash: %s\n", strerror(errno));
            free(usrInputCopy);
            free(cmdArgs);
            return NULL;
        } // failed to allocate. exit.

        Command* command = (Command*)malloc(sizeof(Command));
        if (command == NULL) {
            fprintf(stderr, "yash: %s\n", strerror(errno));
            free(usrInputCopy);
            free(cmdArgs);
            free(job);
            return NULL;
        } // failed to allocate. exit.
        head = command;
        command->argv = cmdArgs;
        command->argCount = 0;
        command->argSize = 10;
        command->inputFile = NULL;
        command->outputFile = NULL;
        command->errorFile = NULL;
        command->next = NULL;
        job->commands = command;
        job->full_command_line = strdup(usrInput);
        job->pgid = 0;
        job->is_background = 0;
        job->state = RUNNING;
        job->next = NULL;

        int commandSaved = 0;
        int isCmd = isCommand(token);
        if ( isCmd == 1 ) {
            cmdArgs[(command->argCount)++] = strdup(token);
            commandSaved = 1;
        } else {
            free(command);
            free(cmdArgs);
            free(usrInputCopy);
            free(job);
            return NULL;
        }

        while ((token = strtok_r(NULL, " ", &saved_token_ptr))) {
            isCmd = isCommand(token);
            if ( isCmd == 1 && !commandSaved) {
                cmdArgs[(command->argCount)++] = strdup(token);
                commandSaved = 1;
            } else if (isCmd == -1) { // ERROR occurred
                cleanup(head, usrInputCopy);
                return NULL;
            } else if ( isFileRedirector(token) ) {
                if ( strcmp(token, ">") == 0 ) {
                    char* fileName = strtok_r(NULL, " ", &saved_token_ptr);
                    command->outputFile = strdup(fileName);
                } else if ( strcmp(token, "<") == 0 ) {
                    char* fileName = strtok_r(NULL, " ", &saved_token_ptr);
                    command->inputFile = strdup(fileName);
                } else {
                    char* fileName = strtok_r(NULL, " ", &saved_token_ptr);
                    command->errorFile = strdup(fileName);
                }
            } else if ( strcmp(token, "|") == 0 ) {
                command->argv[command->argCount] = NULL;

                Command* nextCommand = (Command*)malloc(sizeof(Command));
                if (nextCommand == NULL) {
                    cleanup(command, usrInputCopy);
                    free(job);
                    return NULL;
                }
                command->next = nextCommand;
                command = nextCommand;

                char** newCmdArgs = calloc(10, sizeof(char*));
                if (newCmdArgs == NULL) {
                    fprintf(stderr, "yash: %s\n", strerror(errno));
                    free(nextCommand);
                    cleanup(command, usrInputCopy);
                    free(job);
                    return NULL;
                } // failed to allocate. exit.

                command->argv = newCmdArgs;
                command->argCount = 0;
                command->argSize = 10;
                command->inputFile = NULL;
                command->outputFile = NULL;
                command->errorFile = NULL;
                command->next = NULL;

                cmdArgs = newCmdArgs;
                commandSaved = 0;
            } else if ( strcmp(token, "&") == 0 ) {
                job->is_background = 1;
            } else {
                (command->argCount)++;
                if ((command->argCount) > ((command->argSize) - 1)) { // -1 for NULL
                    (command->argSize)+=5;
                    char** temp = realloc(cmdArgs, sizeof(char*) * ((command->argSize))); // WARNING: Allocated memory is leaked
                    if (temp == NULL) {
                        cleanup(head, usrInputCopy);
                        return NULL;
                    }
                    cmdArgs = temp;
                }
                char* arg_copy = strdup(token);
                cmdArgs[(command->argCount) - 1] = arg_copy;
            }
        }
        cmdArgs[(command->argCount)] = NULL;
    } else {
        free(cmdArgs);
    }
    free(usrInputCopy);
    return job;
}

void launch_job(Job* job) {
    // Determine if shell command
    char* command = job->commands->argv[0];
    pid_t pgid = job->pgid;
    if ( strcmp(command, "fg") == 0) {
        Job* job_to_fg = find_recent_job(job_head);
        if (job_to_fg == NULL) {
            // fprintf(stderr, "yash: %s: %s\n", command, strerror(errno));
            return;
        }

        pid_t target_pgid = job_to_fg->pgid;
        printf("%s\n", job_to_fg->full_command_line);
        update_job_status(job_head, pgid, RUNNING);

        // Send continue signal
        if (kill(-(target_pgid), SIGCONT) < 0) {
            fprintf(stderr, "yash: %s: %s\n", command, strerror(errno));
            return;
        }

        // Setup signal actions
        struct sigaction sa_ign, sa_dfl;
        sa_ign.sa_handler = SIG_IGN;
        sigemptyset(&sa_ign.sa_mask);
        sa_ign.sa_flags = 0;
        sa_dfl.sa_handler = SIG_DFL;
        sigemptyset(&sa_dfl.sa_mask);
        sa_dfl.sa_flags = 0;

        // Ignore terminal signals
        sigaction(SIGTTIN, &sa_ign, NULL);
        sigaction(SIGTTOU, &sa_ign, NULL);

        // Give terminal control to the job
        if (tcsetpgrp(STDIN_FILENO, target_pgid) == -1) {
            fprintf(stderr, "yash: %s: %s\n", command, strerror(errno));
            return;
        }

        // Wait for it to stop or terminate
        int status;
        if (waitpid(-(target_pgid), &status, WUNTRACED) == -1) {
            fprintf(stderr, "yash: %s: %s\n", command, strerror(errno));
            return;
        }

        // Take terminal control back
        if (tcsetpgrp(STDIN_FILENO, getpgrp()) == -1) {
            fprintf(stderr, "yash: %s: %s\n", command, strerror(errno));
            return;
        }

        // Restore terminal signals
        sigaction(SIGTTIN, &sa_dfl, NULL);
        sigaction(SIGTTOU, &sa_dfl, NULL);

        // Handle the result
        if (WIFSTOPPED(status)) {
            update_job_status(job_head, target_pgid, STOPPED);
        } else {
            terminate_job(&job_head, job_to_fg->jobid);
        }
        return;

    }else if (strcmp(command, "bg") == 0 ) {
        Job* job_to_bg = find_recent_job(job_head);
        if (job_to_bg == NULL) {
            printf("yash: bg: current: no such job\n");
            return;
        }

        pid_t target_pgid = job_to_bg->pgid;
        update_job_status(job_head, target_pgid, RUNNING);
        printf("[%d]+ %s &\n", job_to_bg->jobid, job_to_bg->full_command_line);

        if (kill(-(target_pgid), SIGCONT) < 0) {
            fprintf(stderr, "yash: %s: %s\n", command, strerror(errno));
            return;
        }
        return;

    }else if (strcmp(command, "jobs") ==0 ) {
        print_jobs(&job_head);
        return;
    }

    pid_t pid;
    int pipe_fds[2];
    int infile = STDIN_FILENO; // Input for next command
    int outfile = STDOUT_FILENO; // Output for current command

    Command* cmd = job->commands;
    if (cmd == NULL) return;

    while (cmd != NULL) {
        if (cmd->next) { // more than one command present in job
            if (pipe(pipe_fds) < 0) {
                perror("pipe");
                return;
            }
            outfile = pipe_fds[1];
        } else {
            outfile = STDOUT_FILENO;
        }

        pid = fork(); // spawn the child :)

        if (pid < 0) {
            perror("fork");
            return;
        }

        if (pid == 0) {
            /* ------CHILD------ */

            // Reset signal handlers to default behavior for children
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            signal(SIGTTIN, SIG_DFL);
            signal(SIGTTOU, SIG_DFL);
            signal(SIGCHLD, SIG_DFL);

            // Set the PGID
            if (job->pgid == 0) {
                job->pgid = getpid();
            }
            setpgid(0, job->pgid);

            // Redirect STDIN
            if (infile != STDIN_FILENO) {
                dup2(infile, STDIN_FILENO);
                close(infile);
            }

            if (outfile != STDOUT_FILENO) {
                dup2(outfile, STDOUT_FILENO);
                close(outfile);
            }

            // Handle file redirections specified by <, >, 2>
            char* inputFileName = cmd->inputFile;
            if (inputFileName) {
                int fd = open(inputFileName, O_RDONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);

                if ( fd < 0 ) {
                    if ( errno == ENOENT) {
                        fprintf(stderr, "yash: %s: %s\n", inputFileName, strerror(errno));
                    } else {
                        fprintf(stderr, "yash: %s: %s\n", inputFileName, strerror(errno));
                    }
                    // commandSaved = 0;
                    break;
                }
                if ( dup2(fd, STDIN_FILENO) < 0 ) {
                    fprintf(stderr, "yash: %s: %s\n", inputFileName, strerror(errno));
                    close(fd);
                    exit(1);
                }
                // stdin_redirect = 1;
                close(fd);
            }

            char* outputFileName = cmd->outputFile;
            if (outputFileName) {
                int fd = open(outputFileName, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);

                if ( fd < 0) {
                    fprintf(stderr, "yash: %s: %s\n", outputFileName, strerror(errno));
                    // commandSaved = 0;
                    break;
                }

                if ( dup2(fd, STDOUT_FILENO) < 0 ) {
                    fprintf(stderr, "yash: %s: %s\n", outputFileName, strerror(errno));
                    close(fd);
                    // commandSaved = 0;
                    break;
                }
                // stdout_redirect = 1;
                close(fd);
            }

            char* errorFileName = cmd->errorFile;
            if (errorFileName) {
                int fd = open(errorFileName, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);

                if ( fd < 0) {
                    fprintf(stderr, "yash: %s: %s\n", errorFileName, strerror(errno));
                    // commandSaved = 0;
                    break;
                }

                if ( dup2(fd, STDERR_FILENO) < 0 ) {
                    fprintf(stderr, "yash: %s: %s\n", errorFileName, strerror(errno));
                    close(fd);
                    // commandSaved = 0;
                    break;
                }
                // stderr_redirect = 1;
                close(fd);
            }

            // Execute command
            execvp(cmd->argv[0], cmd->argv);
            perror("execvp");
            exit(1);
        }

        /* ------PARENT------ */
        if (job->pgid == 0) {
            job->pgid = pid;
        }

        // Clean up file descriptors
        if (infile != STDIN_FILENO) {
            close(infile);
        }

        if (outfile != STDOUT_FILENO) {
            close(outfile);
        }

        // Input for next command
        infile = pipe_fds[0];

        cmd = cmd->next;
    }

    insert_job(&job_head, job);
    if (!(job->is_background)) {
        struct sigaction sa_ign, sa_dfl;

        // Setup for ignoring signals
        sa_ign.sa_handler = SIG_IGN;
        sigemptyset(&sa_ign.sa_mask);
        sa_ign.sa_flags = 0;

        // Setup for restoring default signal handlers
        sa_dfl.sa_handler = SIG_DFL;
        sigemptyset(&sa_dfl.sa_mask);
        sa_dfl.sa_flags = 0;

        // Temporarily ignore terminal stop signals
        sigaction(SIGTTIN, &sa_ign, NULL);
        sigaction(SIGTTOU, &sa_ign, NULL);

        g_foreground_pgid = job->pgid;
        // Gives terminal control to child
        if (tcsetpgrp(STDIN_FILENO, job->pgid) == -1) {
            fprintf(stderr, "yash: %s: %s\n", command,strerror(errno));
            return;
        }

        // Wait for child to terminate
        int status;
        if (waitpid(job->pgid, &status, WUNTRACED) == -1) {
            fprintf(stderr, "yash: %s: %s\n", command,strerror(errno));
            return;
        }

        // Child changed state, terminal control regained by shell
        if (tcsetpgrp(STDIN_FILENO, getpgrp()) == -1) {
            fprintf(stderr, "yash: %s: %s\n", command,strerror(errno));
            return;
        }

        g_foreground_pgid = 0;
        // Restore the default handlers for the terminal stop signals.
        sigaction(SIGTTIN, &sa_dfl, NULL);
        sigaction(SIGTTOU, &sa_dfl, NULL);

        // Child finished normally, remove from job list
        if (WIFSTOPPED(status)) {
            update_job_status(job_head, job->pgid, STOPPED);
        }else {
            terminate_job(&job_head, job->jobid);
        }
    }

}

int main(int argc, char *argv[]) {
    struct sigaction sa;

    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    // Registering the handler
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        fprintf(stderr, "yash: %s\n", strerror(errno));
        exit(1);
    }

    sa.sa_handler = handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    // Registering the handler
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        fprintf(stderr, "yash: %s\n", strerror(errno));
        exit(1);
    }

    sa.sa_handler = handle_sigtstp;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    // Registering the handler
    if (sigaction(SIGTSTP, &sa, NULL) == -1) {
        fprintf(stderr, "yash: %s\n", strerror(errno));
        exit(1);
    }

    // The shell itself must IGNORE SIGTSTP (Ctrl+Z).
    // This prevents the shell from stopping itself.
    sa.sa_handler = SIG_IGN; // Set the action to Ignore
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGTSTP, &sa, NULL) == -1) {
        fprintf(stderr, "yash: %s\n", strerror(errno));
        exit(1);
    }

    char* usrInput;

    while ((usrInput = readline("# "))) { // if user enters ^D
        if (usrInput[0] == '\0') {
            free(usrInput);
            continue;
        }

        Job* job = parse_line(usrInput);

        if (job) {
            launch_job(job);
        }

        free(usrInput);
    }
    free_jobs(&job_head);
    return 0;
}
