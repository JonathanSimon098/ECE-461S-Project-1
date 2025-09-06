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


            while ((token = strtok(NULL, " "))) {
                isCmd = isCommand(token);
                if ( isCmd == 1 && !commandSaved) {
                    command = strdup(token);
                    if (command == NULL) exit(1);
                    cmdArgs[cmdArgsIndex++] = command;
                    commandExecuted = 0;
                    commandSaved = 1;
                } else if (isCmd == -1) { // ERROR occurred
                    free(usrInputCopy);
                    free(usrInput);
                    free(cmdArgs);
                    exit(1);
                } else if ( isFileRedirector(token) ) {
                    // TODO: Add file redirection logic
                } else if ( strcmp(token, "|") == 0 ) {
                    // TODO: Add piping logic
                } else {
                    cmdArgsIndex++;
                    if (cmdArgsIndex > (cmdArgsCount - 1)) { // -1 for NULL
                        cmdArgsCount+=5;
                        char** temp = realloc(cmdArgs, sizeof(char*) * (cmdArgsCount)); // WARNING: Allocated memory is leaked
                        if (temp == NULL) {
                            for (int i = 0; i < cmdArgsIndex; i++) free(cmdArgs[i]);
                            free(cmdArgs);
                            free(usrInputCopy);
                            free(usrInput);
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
                // Determine if shell command
                if ( strcmp(command, "fg") == 0) {

                }else if (strcmp(command, "bg") == 0 ) {

                }else if (strcmp(command, "jobs") ==0 ) {

                } else {
                    // CREATE FORK AND THEN EXEC
                    const pid_t pid = fork();

                    if ( pid == 0 ) {
                        /* CHILD */
                        execvp(command, cmdArgs);
                    }
                    wait(NULL);
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
