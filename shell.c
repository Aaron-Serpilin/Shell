#include "parser/ast.h"
#include "shell.h"
#include <stdio.h>  // for getc, printf
#include <stdlib.h> // malloc, free
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

void initialize(void) {

    if (prompt) {
        prompt = "shellLine$ ";
    }
        
}

void run_command(node_t *node) {

    if (prompt) {
        prompt = "shellLine$";
    }

    //print_tree(node);

    switch (node->type) {
        case NODE_COMMAND: {

            char *shellCommand = node->command.program;
            char **argv = node->command.argv;
            int status;

            if (strcmp(shellCommand, "exit") == 0) {

                if (node->command.argc > 1) {
                    exit(atoi(argv[1]));
                }

            } else if (strcmp(shellCommand, "cd") == 0) {

                chdir(argv[1]);

            } else {
                pid_t childProcess = fork();

                if (childProcess == 0) { // Means the current process is a child
                    execvp(shellCommand, argv);
                } else if (childProcess > 0) { // This if for parent process that wait the child to return some sort of data, and then kill the process
                    waitpid(childProcess, &status, 0);
                } else {
                    perror("Pid Error");
                }
            }

            break;

        }
        case NODE_DETACH:
            break;
        case NODE_PIPE:
            break;
        case NODE_REDIRECT:
            break;
        case NODE_SEQUENCE:
            break;
        case NODE_SUBSHELL:
            break;
        default:
            break;
    }
}
