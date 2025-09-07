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

Node* jobs_head = NULL;
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

int execute_command(char* usrInput, char* command, char** cmdArgs, int stdin_redirect, int stdout_redirect, int stderr_redirect, int saved_stdin, int saved_stdout, int saved_stderr
    , int active_pipe, pid_t* pipe_pgid, int pipe_detected, int pipe_fds[], int temp_fds_holder[], int send_to_bg, JobState state) {
    // Determine if shell command
    if ( strcmp(command, "fg") == 0) {
        Node* job_to_fg = find_last_stopped_job(jobs_head);
        if (job_to_fg == NULL) {
            fprintf(stderr, "yash: %s: %s\n", command, strerror(errno));
            return -2;
        }

        printf("%s\n", job_to_fg->command);
        update_job_status(jobs_head, job_to_fg->pid, RUNNING);

        // Send continue signal
        if (kill(-(job_to_fg->pgid), SIGCONT) < 0) {
            fprintf(stderr, "yash: %s: %s\n", command, strerror(errno));
            return -1;
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
        if (tcsetpgrp(STDIN_FILENO, job_to_fg->pgid) == -1) {
            fprintf(stderr, "yash: %s: %s\n", command, strerror(errno));
            return -1;
        }

        // Wait for it to stop or terminate
        int status;
        if (waitpid(-(job_to_fg->pid), &status, WUNTRACED) == -1) {
            fprintf(stderr, "yash: %s: %s\n", command, strerror(errno));
            return -1;
        }

        // Take terminal control back
        if (tcsetpgrp(STDIN_FILENO, getpgrp()) == -1) {
            fprintf(stderr, "yash: %s: %s\n", command, strerror(errno));
            return -1;
        }

        // Restore terminal signals
        sigaction(SIGTTIN, &sa_dfl, NULL);
        sigaction(SIGTTOU, &sa_dfl, NULL);

        // Handle the result
        if (WIFSTOPPED(status)) {
            update_job_status(jobs_head, job_to_fg->pid, STOPPED);
        } else {
            terminate_job(&jobs_head, job_to_fg->jobid);
        }

    }else if (strcmp(command, "bg") == 0 ) {
        Node* job_to_bg = find_last_stopped_job(jobs_head);
        if (job_to_bg == NULL) {
            printf("yash: bg: current: no such job\n");
            return -2;
        }

        update_job_status(jobs_head, job_to_bg->pid, RUNNING);
        printf("[%d]+ %s &\n", job_to_bg->jobid, job_to_bg->command);

        if (kill(-(job_to_bg->pid), SIGCONT) < 0) {
            fprintf(stderr, "yash: %s: %s\n", command, strerror(errno));
            return -1;
        }

    }else if (strcmp(command, "jobs") ==0 ) {
        print_jobs(&jobs_head, jobs_head, 1);
    } else {
        // CREATE FORK AND THEN EXEC
        const pid_t pid = fork();
        if ( pid == -1 ) {
            fprintf(stderr, "yash: %s: %s\n", command, strerror(errno));
            return -1;
        }

        if ( pid == 0 ) {
            /* CHILD */

            pid_t pgid_to_set = (*pipe_pgid == 0) ? getpid() : *pipe_pgid;
            if (setpgid(0, pgid_to_set) == -1) {
                fprintf(stderr, "yash: %s: %s\n", command, strerror(errno));
                return -1;
            }

            if ( active_pipe ) {
                // cleanup old pipe and start new
                if ( dup2(pipe_fds[0], STDIN_FILENO) < 0 ) {
                    fprintf(stderr, "yash: %s: %s\n", command, strerror(errno));
                    close(pipe_fds[0]);
                    close(pipe_fds[1]);
                    return -2;
                }
                close(pipe_fds[0]);
                //close(pipe_fds[1]);
            }
            // currently no pipe active, use new one
            if ( pipe_detected ) {
                if ( dup2(temp_fds_holder[1], STDOUT_FILENO) < 0 ) {
                    fprintf(stderr, "yash: %s: %s\n", command, strerror(errno));
                    close(temp_fds_holder[0]);
                    close(temp_fds_holder[1]);
                    return -2;
                }
                close(temp_fds_holder[1]);
            }
            // Reset signals for child
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            signal(SIGTTIN, SIG_DFL);
            signal(SIGTTOU, SIG_DFL);
            signal(SIGCHLD, SIG_DFL);
            execvp(command, cmdArgs);
            //pipe_fds[0] = temp_fds_holder[0];
        } else {
            /* PARENT */
            // parent does not need pipe
            if (*pipe_pgid == 0) {
                *pipe_pgid = pid;
            }

            if (setpgid(pid, *pipe_pgid) == -1) {
                fprintf(stderr, "yash: %s: %s\n", command, strerror(errno));
                return -1;
            }

            if ( pipe_detected ) {
                //close(temp_fds_holder[0]);
                close(temp_fds_holder[1]);
                pipe_fds[0] = temp_fds_holder[0];
            } else {
                // Restores STDs if end of pipe sequence
                if ( stdin_redirect ) {
                    if ( dup2(saved_stdin, STDIN_FILENO) < 0 ) {
                        fprintf(stderr, "yash: %s: %s\n", command, strerror(errno));
                        return -1;
                    }
                    close(saved_stdin);
                }
                if ( stdout_redirect ) {
                    if ( dup2(saved_stdout, STDOUT_FILENO) < 0 ) {
                        fprintf(stderr, "yash: %s: %s\n", command,strerror(errno));
                        return -1;
                    }
                    close(saved_stdout);
                }
                if ( stderr_redirect ) {
                    if ( dup2(saved_stderr, STDERR_FILENO) < 0 ) {
                        fprintf(stderr, "yash: %s: %s\n", command,strerror(errno));
                        return -1;
                    }
                    close(saved_stderr);
                }
            }
        }
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

        int jobid = insert_job(&jobs_head, pid, *pipe_pgid, usrInput, state);
        if (send_to_bg) {
            // printf("[%d]+ %s\n", jobid, usrInput);
        } else {
            g_foreground_pgid = *pipe_pgid;
            // Gives terminal control to child
            if (tcsetpgrp(STDIN_FILENO, *pipe_pgid) == -1) {
                fprintf(stderr, "yash: %s: %s\n", command,strerror(errno));
                return -1;
            }

            // Wait for child to terminate
            int status;
            if (waitpid(pid, &status, WUNTRACED) == -1) {
                fprintf(stderr, "yash: %s: %s\n", command,strerror(errno));
                return -1;
            }

            // Child changed state, terminal control regained by shell
            if (tcsetpgrp(STDIN_FILENO, getpgrp()) == -1) {
                fprintf(stderr, "yash: %s: %s\n", command,strerror(errno));
                return -1;
            }

            g_foreground_pgid = 0;
            // Restore the default handlers for the terminal stop signals.
            sigaction(SIGTTIN, &sa_dfl, NULL);
            sigaction(SIGTTOU, &sa_dfl, NULL);

            // Child finished normally, remove from job list
            if (WIFSTOPPED(status)) {
                update_job_status(jobs_head, pid, STOPPED);
            }else {
                terminate_job(&jobs_head, jobid);
            }
        }
    }

    return 0;
}

void cleanup(int cmdArgsIndex, char** cmdArgs, char* usrInputCopy, char* usrInput) {
    for (int i = 0; i < cmdArgsIndex; i++) free(cmdArgs[i]);
    free(cmdArgs);
    free(usrInputCopy);
    free(usrInput);
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
            update_job_status(jobs_head, pid, STOPPED);
        } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
            update_job_status(jobs_head, pid, DONE);
        } else if (WIFCONTINUED(status)) {
            update_job_status(jobs_head, pid, RUNNING);
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
    int commandExecuted = 0;
    char** cmdArgs = calloc(10, sizeof(char*));
    if (cmdArgs == NULL) exit(1); // failed to allocate. exit.

    int cmdArgsCount = 10;

    while ((usrInput = readline("# "))) { // if user enters ^D
        if (usrInput[0] == '\0') {
            free(usrInput);
            continue;
        }

        char* usrInputCopy = strdup(usrInput);
        if (usrInputCopy == NULL) {
            fprintf(stderr, "yash: %s\n", strerror(errno));
            free(usrInput);
            free(cmdArgs);
            return 1;
        }

        char* saved_token_ptr;
        char* token = strtok_r(usrInputCopy, " ", &saved_token_ptr);
        char* command = strdup(token);
        if (token != NULL && command != NULL) {
            int cmdArgsIndex = 0;
            int commandSaved = 0;
            int isCmd = isCommand(token);
            if ( isCmd == 1 ) {
                cmdArgs[cmdArgsIndex++] = command;
                commandExecuted = 0;
                commandSaved = 1;
            } else if (isCmd == -1) {
                free(command);
                free(cmdArgs);
                free(usrInputCopy);
                free(usrInput);
                exit(1);
            } else {
                free(command);
                free(usrInputCopy);
                free(usrInput);
                continue;
            }

            int saved_stdin = dup(STDIN_FILENO);
            int saved_stdout = dup(STDOUT_FILENO);
            int saved_stderr = dup(STDERR_FILENO);
            int fd;
            int stdin_redirect = 0;
            int stdout_redirect = 0;
            int stderr_redirect = 0;
            int pipe_fds[2];
            int active_pipe = 0;
            pid_t pipe_pgid = 0;
            while ((token = strtok_r(NULL, " ", &saved_token_ptr))) {
                isCmd = isCommand(token);
                if ( isCmd == 1 && !commandSaved) {
                    command = strdup(token);
                    if (command == NULL) exit(1);
                    cmdArgs[cmdArgsIndex++] = command;
                    commandExecuted = 0;
                    commandSaved = 1;
                } else if (isCmd == -1) { // ERROR occurred
                    cleanup(cmdArgsIndex, cmdArgs, usrInputCopy, usrInput);
                    exit(1);
                } else if ( isFileRedirector(token) ) {
                    if ( strcmp(token, ">") == 0 ) {
                        char* fileName = strtok_r(NULL, " ", &saved_token_ptr);
                        fd = open(fileName, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);

                        if ( fd < 0) {
                            fprintf(stderr, "yash: %s: %s\n", fileName, strerror(errno));
                            commandSaved = 0;
                            break;
                        }

                        if ( dup2(fd, STDOUT_FILENO) < 0 ) {
                            fprintf(stderr, "yash: %s: %s\n", fileName, strerror(errno));
                            close(fd);
                            commandSaved = 0;
                            break;
                        }
                        stdout_redirect = 1;
                        close(fd);
                    } else if ( strcmp(token, "<") == 0 ) {
                        char* fileName = strtok_r(NULL, " ", &saved_token_ptr);
                        fd = open(fileName, O_RDONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);

                        if ( fd < 0 ) {
                            if ( errno == ENOENT) {
                                fprintf(stderr, "yash: %s: %s\n", fileName, strerror(errno));
                            } else {
                                fprintf(stderr, "yash: %s: %s\n", fileName, strerror(errno));
                            }
                            commandSaved = 0;
                            break;
                        }
                        if ( dup2(fd, STDIN_FILENO) < 0 ) {
                            fprintf(stderr, "yash: %s: %s\n", fileName, strerror(errno));
                            close(fd);
                            exit(1);
                        }
                        stdin_redirect = 1;
                        close(fd);
                    } else {
                        char* fileName = strtok_r(NULL, " ", &saved_token_ptr);
                        fd = open(fileName, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);

                        if ( fd < 0) {
                            fprintf(stderr, "yash: %s: %s\n", fileName, strerror(errno));
                            commandSaved = 0;
                            break;
                        }

                        if ( dup2(fd, STDERR_FILENO) < 0 ) {
                            fprintf(stderr, "yash: %s: %s\n", fileName, strerror(errno));
                            close(fd);
                            commandSaved = 0;
                            break;
                        }
                        stderr_redirect = 1;
                        close(fd);
                    }
                } else if ( strcmp(token, "|") == 0 ) {
                    int temp_fds_holder[2];
                    if (pipe(temp_fds_holder) == -1) {
                        fprintf(stderr, "yash: %s\n", strerror(errno));
                        exit(1);
                    }

                    int exec_status = execute_command(usrInput, command, cmdArgs, stdin_redirect, stdout_redirect, stderr_redirect,
                        saved_stdin, saved_stdout, saved_stderr, active_pipe, &pipe_pgid, 1, pipe_fds, temp_fds_holder, 0, RUNNING);
                    if (exec_status == -1) {
                        cleanup(cmdArgsIndex, cmdArgs, usrInputCopy, usrInput);
                        exit(1);
                    }
                    if (exec_status == -2) break;
                    commandSaved = 0;
                    for (int i = 0; i < cmdArgsIndex; i++) free(cmdArgs[i]);
                    cmdArgsIndex = 0;
                    active_pipe = 1;
                } else if ( strcmp(token, "&") == 0 ) {
                    int exec_status = execute_command(usrInput, command, cmdArgs, stdin_redirect, stdout_redirect, stderr_redirect,
                        saved_stdin, saved_stdout, saved_stderr, active_pipe, &pipe_pgid, 0, pipe_fds, NULL, 1, RUNNING);
                    if (exec_status == -1) {
                        cleanup(cmdArgsIndex, cmdArgs, usrInputCopy, usrInput);
                        exit(1);
                    }
                    if (exec_status == -2) break;
                    commandSaved = 0;
                    for (int i = 0; i < cmdArgsIndex; i++) free(cmdArgs[i]);
                    cmdArgsIndex = 0;
                } else {
                    cmdArgsIndex++;
                    if (cmdArgsIndex > (cmdArgsCount - 1)) { // -1 for NULL
                        cmdArgsCount+=5;
                        char** temp = realloc(cmdArgs, sizeof(char*) * (cmdArgsCount)); // WARNING: Allocated memory is leaked
                        if (temp == NULL) {
                            cleanup(cmdArgsIndex, cmdArgs, usrInputCopy, usrInput);
                            exit(1);
                        }
                        cmdArgs = temp;

                    }
                    char* arg_copy = strdup(token);
                    cmdArgs[cmdArgsIndex - 1] = arg_copy;
                }
            }
            cmdArgs[cmdArgsIndex] = NULL;
            if (!commandExecuted && commandSaved) {
                int exec_status = execute_command(usrInput, command, cmdArgs, stdin_redirect, stdout_redirect, stderr_redirect,
                        saved_stdin, saved_stdout, saved_stderr, active_pipe, &pipe_pgid, 0, pipe_fds, NULL, 0, RUNNING);
                if (exec_status == -1) {
                    cleanup(cmdArgsIndex, cmdArgs, usrInputCopy, usrInput);
                    exit(1);
                }
            }
            for (int i = 0; i < cmdArgsIndex; i++) free(cmdArgs[i]);
        }
        free(usrInputCopy);
        free(usrInput);
    }
    free_jobs(&jobs_head);
    free(cmdArgs);
    return 0;
}
