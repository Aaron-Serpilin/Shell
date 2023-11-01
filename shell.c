#include "parser/ast.h"
#include "shell.h"
#include <stdio.h>  // for getc, printf
#include <stdlib.h> // malloc, free

void initialize(void) {

    if (prompt) {
        prompt = "shellLine$ ";
    }
        
}

void run_command(node_t *node) {

    if (prompt) {
        prompt = "shellLine$";
    }

    print_tree(node);

    switch (node->type) {
        case NODE_COMMAND: {

            char *shellCommand = node->command.program;
            char **argv = node->command.argv;
            int status;
            pid_t pid = fork();

            if (pid == 0) { // Means the current process is a child
                execvp(shellCommand, argv);
                exit(1);
            } else if (pid > 0) { // This if for parent process that wait the child to return some sort of data, and then kill the process
                waitpid(pid, &status, 0);
            } else {
                perror("Pid Error");
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


