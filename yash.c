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

int main(int argc, char *argv[]) {

    char* usrInput = readline("# ");

    while (usrInput  != NULL) {
        usrInput = readline("# ");
    }

    free(usrInput);
    return 0;
}
