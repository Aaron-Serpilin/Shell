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

void execute_command (pid_t process, char* command, char** arguments, int status) {

    if (process == 0) { // Means the current process is a child
        execvp(command, arguments);
        perror(NULL);
    } else if (process > 0) { // This if for parent process that wait the child to return some sort of data, and then kill the process
        waitpid(process, &status, 0);
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
            int status = 0;

            if (strcmp(shellCommand, "exit") == 0) { // Actionable commands are done in the parent and do not require forking protection

                if (node->command.argc > 1) {
                    exit(atoi(argv[1]));
                }

            } else if (strcmp(shellCommand, "cd") == 0) {

                chdir(argv[1]);

            } else { // This applies to any command that returns value. We make a child to not overwrite the parent

                pid_t childProcess = fork();
                execute_command(childProcess, shellCommand, argv, status);

            }

            break;

        }
        case NODE_DETACH:
            break;
        case NODE_PIPE:
            break;
        case NODE_REDIRECT:
            break;
        case NODE_SEQUENCE:{

            run_command(node->sequence.first);
            run_command(node->sequence.second);

            break;
        }
  
        case NODE_SUBSHELL:
            break;
        default:
            break;
    }
}
