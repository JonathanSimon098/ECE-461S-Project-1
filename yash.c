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
        exit(1);
    }
    char* directory = strtok(path_env, ":");
    while ( directory != NULL) {
        const int sizeOfDirectory = (int)(strlen(directory) + strlen(tkn) + 2); // +2 for '/' and \0
        char cmdDirectory[sizeOfDirectory];
        snprintf(cmdDirectory, sizeOfDirectory, "%s/%s", directory, tkn);

        if (access(cmdDirectory, X_OK)) {
            return 1;
        }

        directory = strtok(NULL, ":");
    }
    //free(path_env);
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

int tokenType( char* tkn ) {
    if ( isCommand(tkn)) {
        return 0;
    } else if ( isFileRedirector(tkn) ) {
        return 2;
    } else if ( strcmp(tkn, "|") == 0 ) {
        return 3;
    } else {
        return 1;
    }
}

int main(int argc, char *argv[]) {
    char* usrInput;
    int commandExecuted = 0;
    char* command;
    char** cmdArgs;

    while ((usrInput = readline("# "))) { // if user enters ^D
        if (usrInput[0] == '\0') {
            free(usrInput);
            continue;
        }

        char* usrInputCopy = strdup(usrInput);
        if (usrInputCopy == NULL) {
            printf("Memory failed to allocate.\n");
            free(usrInput);
            return 1;
        }

        char* token = strtok(usrInputCopy, " ");
        if (token != NULL) {
            int commandSaved = 0;
            if (isCommand(token)) {
                command = strdup(token);
                commandExecuted = 0;
                commandSaved = 1;
            }else {
                continue;
            }


            while ((token = strtok(NULL, " "))) {
                switch (tokenType(token)) {
                    case 0: // Command
                        if (!commandSaved) {
                            command = strdup(token);
                            commandExecuted = 0;
                            commandSaved = 1;
                            break;
                        }
                    case 1: // Argument

                        break;
                    case 2: // File redirection
                        break;
                    case 3: // Piping
                        break;
                    default:
                        // invalid token, should exit this line iteration
                }
                printf("Args: %s\n", token);
            }
            if (!commandExecuted && commandSaved) {
                // Determine if shell command
                if ( strcmp(command, "fg") == 0) {

                }else if (strcmp(command, "bg") == 0 ) {

                }else if (strcmp(command, "jobs") ==0 ) {

                } else {
                    // CREATE FORK AND THEN EXEC
                    // execvp(command, cmdArgs);
                }
                commandSaved = 0;
                commandExecuted = 1;
            }
        }

        free(usrInputCopy);
        free(usrInput);
    }
    return 0;
}
