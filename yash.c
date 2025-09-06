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
#include <sys/wait.h>
#include <string.h>
#include <unistd.h>

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

int execute_command(char* command, char** cmdArgs, int stdin_redirect, int stdout_redirect, int stderr_redirect, int saved_stdin, int saved_stdout, int saved_stderr
    , int active_pipe, int* pipe_pgid, int pipe_detected, int pipe_fds[], int temp_fds_holder[]) {
    // Determine if shell command
    if ( strcmp(command, "fg") == 0) {

    }else if (strcmp(command, "bg") == 0 ) {

    }else if (strcmp(command, "jobs") ==0 ) {

    } else {
        // CREATE FORK AND THEN EXEC
        const pid_t pid = fork();
        if ( pid == -1 ) {
            printf("Error forking\n");
            return -1;
        }

        if ( pid == 0 ) {
            /* CHILD */
            if ( active_pipe ) {
                if (setpgid(0, *pipe_pgid) == -1) {
                    printf("Error setting pgid to child\n");
                    return -1;
                }
            } else {
                *pipe_pgid = pid;
                if (setpgid(0, 0) == -1) {
                    printf("Error setting pgid of child\n");
                    return -1;
                }
            }

            if ( active_pipe ) {
                // cleanup old pipe and start new
                if ( dup2(pipe_fds[0], STDIN_FILENO) < 0 ) {
                    printf("Failed to redirect STDIN\n");
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
                    printf("Failed to redirect STDOUT\n");
                    close(temp_fds_holder[0]);
                    close(temp_fds_holder[1]);
                    return -2;
                }
                close(temp_fds_holder[1]);
            }
            execvp(command, cmdArgs);
            //pipe_fds[0] = temp_fds_holder[0];
        } else {
            // parent does not need pipe
            if ( pipe_detected ) {
                //close(temp_fds_holder[0]);
                close(temp_fds_holder[1]);
                pipe_fds[0] = temp_fds_holder[0];
            } else {
                // Restores STDs if end of pipe sequence
                if ( stdin_redirect ) {
                    if ( dup2(saved_stdin, STDIN_FILENO) < 0 ) {
                        printf("Failed to restore stdin\n");
                        return -1;
                    }
                    close(saved_stdin);
                }
                if ( stdout_redirect ) {
                    if ( dup2(saved_stdout, STDOUT_FILENO) < 0 ) {
                        printf("Failed to restore stdout\n");
                        return -1;
                    }
                    close(saved_stdout);
                }
                if ( stderr_redirect ) {
                    if ( dup2(saved_stderr, STDERR_FILENO) < 0 ) {
                        printf("Failed to restore stderr\n");
                        return -1;
                    }
                    close(saved_stderr);
                }
            }
        }
        if ( !pipe_detected ) {
            while (wait(NULL) > 0 || errno != ECHILD); // wait for all children to terminate
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

int main(int argc, char *argv[]) {
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
            printf("Memory failed to allocate.\n");
            free(usrInput);
            free(cmdArgs);
            return 1;
        }

        char* token = strtok(usrInputCopy, " ");
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
            int pipe_pgid = 0;
            while ((token = strtok(NULL, " "))) {
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
                        char* fileName = strtok(NULL, " ");
                        fd = open(fileName, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);

                        if ( fd < 0) {
                            printf("yash: File %s could not be opened\n", fileName); // Remove? Expected error msg?
                            commandSaved = 0;
                            break;
                        }

                        if ( dup2(fd, STDOUT_FILENO) < 0 ) {
                            printf("Failed to redirect STDOUT\n");
                            close(fd);
                            commandSaved = 0;
                            break;
                        }
                        stdout_redirect = 1;
                        close(fd);
                    } else if ( strcmp(token, "<") == 0 ) {
                        char* fileName = strtok(NULL, " ");
                        fd = open(fileName, O_RDONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);

                        if ( fd < 0 ) {
                            if ( errno == ENOENT) {
                                printf("yash: File %s does not exist\n", fileName); // Remove? Expected error msg?
                            } else {
                                printf("yash: File %s could not be opened\n", fileName);
                            }
                            commandSaved = 0;
                            break;
                        }
                        if ( dup2(fd, STDIN_FILENO) < 0 ) {
                            printf("Failed to redirect STDIN\n");
                            close(fd);
                            exit(1);
                        }
                        stdin_redirect = 1;
                        close(fd);
                    } else {
                        char* fileName = strtok(NULL, " ");
                        fd = open(fileName, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);

                        if ( fd < 0) {
                            printf("yash: File %s could not be opened\n", fileName); // Remove? Expected error msg?
                            commandSaved = 0;
                            break;
                        }

                        if ( dup2(fd, STDERR_FILENO) < 0 ) {
                            printf("Failed to redirect STDERR\n");
                            close(fd);
                            commandSaved = 0;
                            break;
                        }
                        stderr_redirect = 1;
                        close(fd);
                    }
                } else if ( strcmp(token, "|") == 0 ) {
                    // TODO: Add piping logic
                    int temp_fds_holder[2];
                    if (pipe(temp_fds_holder) == -1) {
                        printf("Error creating pipe\n");
                        exit(1);
                    }

                    // // Determine if shell command
                    // if ( strcmp(command, "fg") == 0) {
                    //
                    // }else if (strcmp(command, "bg") == 0 ) {
                    //
                    // }else if (strcmp(command, "jobs") ==0 ) {
                    //
                    // } else {
                    //     // CREATE FORK AND THEN EXEC
                    //     const pid_t pid = fork();
                    //     if ( pid == -1 ) {
                    //         printf("Error forking\n");
                    //         cleanup(cmdArgsIndex, cmdArgs, usrInputCopy, usrInput);
                    //         exit(1);
                    //     }
                    //
                    //     if ( active_pipe ) {
                    //         if (setpgid(0, pipe_pgid) == -1) {
                    //             printf("Error setting pgid to child\n");
                    //             cleanup(cmdArgsIndex, cmdArgs, usrInputCopy, usrInput);
                    //             exit(1);
                    //         }
                    //     } else {
                    //         pipe_pgid = pid;
                    //         if (setpgid(0, 0) == -1) {
                    //             printf("Error setting pgid of child\n");
                    //             cleanup(cmdArgsIndex, cmdArgs, usrInputCopy, usrInput);
                    //             exit(1);
                    //         }
                    //     }
                    //
                    //     if ( pid == 0 ) {
                    //         /* CHILD */
                    //         if ( active_pipe ) {
                    //             // cleanup old pipe and start new
                    //             if ( dup2(pipe_fds[0], STDIN_FILENO) < 0 ) {
                    //                 printf("Failed to redirect STDIN\n");
                    //                 close(pipe_fds[0]);
                    //                 close(pipe_fds[1]);
                    //                 commandSaved = 0;
                    //                 break;
                    //             }
                    //             close(pipe_fds[0]);
                    //             close(pipe_fds[1]);
                    //         }
                    //         // currently no pipe active, use new one
                    //         if ( dup2(temp_fds_holder[1], STDOUT_FILENO) < 0 ) {
                    //             printf("Failed to redirect STDOUT\n");
                    //             close(temp_fds_holder[0]);
                    //             close(temp_fds_holder[1]);
                    //             commandSaved = 0;
                    //             break;
                    //         }
                    //         close(temp_fds_holder[1]);
                    //         execvp(command, cmdArgs);
                    //     } else {
                    //         // parent does not need pipe
                    //         //close(temp_fds_holder[0]);
                    //         close(temp_fds_holder[1]);
                    //     }
                    //     // wait(NULL);
                    // }
                    // pipe_fds[0] = temp_fds_holder[0];
                    int exec_status = execute_command(command, cmdArgs, stdin_redirect, stdout_redirect, stderr_redirect,
                        saved_stdin, saved_stdout, saved_stderr, active_pipe, &pipe_pgid, 1, pipe_fds, temp_fds_holder);
                    if (exec_status == -1) {
                        cleanup(cmdArgsIndex, cmdArgs, usrInputCopy, usrInput);
                        exit(1);
                    }
                    if (exec_status == -2) break;
                    commandSaved = 0;
                    for (int i = 0; i < cmdArgsIndex; i++) free(cmdArgs[i]);
                    cmdArgsIndex = 0;
                    active_pipe = 1;
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
                int exec_status = execute_command(command, cmdArgs, stdin_redirect, stdout_redirect, stderr_redirect,
                        saved_stdin, saved_stdout, saved_stderr, active_pipe, &pipe_pgid, 0, pipe_fds, NULL);
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
    free(cmdArgs);
    return 0;
}
