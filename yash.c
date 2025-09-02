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
        strcmp(tkn, "echo") == 0 ||
        strcmp(tkn, "ls") == 0
        ) {
        return 1;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    char* usrInput;

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
            printf("Command: %s\n", token);
            while ((token = strtok(NULL, " "))) {
                printf("Args: %s\n", token);
            }
        }

        free(usrInputCopy);
        free(usrInput);
    }
    return 0;
}
