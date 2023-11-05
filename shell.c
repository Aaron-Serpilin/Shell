#include "parser/ast.h"
#include "shell.h"
#include <stdio.h>  
#include <stdlib.h> 
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>

#define PIPE_READ_FUNCTION 0
#define PIPE_WRITE_FUNCTION 1
#define STDIN 0
#define STDOUT 1

void initialize(void) {

    if (prompt) {
        prompt = "shellLine$ ";
    }
        
}

void signal_handler (int signum) {
    printf("Interrupt Triggered. Terminating terminal with signal %d\n", signum);
}

void execute_command (pid_t process, char* command, char** arguments, int status) {

    if (process == 0) { // Means the current process is a child
        execvp(command, arguments);
        perror(NULL); // Acts like a catch statement and returns the corresponding error if the execution fails
    } else if (process > 0) { // This if for parent process that wait the child to return some sort of data, and then kill the process
        waitpid(process, &status, 0);
    } 
    
}

void run_pipe(node_t *node) {
    int number_pipe_parts = node->pipe.n_parts;
    int status = 0;
    pid_t current_process;

    int fd[number_pipe_parts - 1][2]; // Array of pipes

    for (int i = 0; i < number_pipe_parts; i++) {

        if (i < number_pipe_parts - 1) {
            if (pipe(fd[i]) == -1) {
                perror(NULL);
            }
        }

        current_process = fork(); // Each process is carried out in its own fork

        if (current_process == 0) { // Child Process
            // When iterating through fd[i][0/1], we iterate through the different pipes that connect the multiple commands
           if (i > 0) { 

                close(fd[i - 1][PIPE_WRITE_FUNCTION]);
                dup2(fd[i - 1][PIPE_READ_FUNCTION], STDIN);
                close(fd[i - 1][PIPE_READ_FUNCTION]);

            } else if (i < number_pipe_parts - 1) { 

                close(fd[i][PIPE_READ_FUNCTION]);
                dup2(fd[i][PIPE_WRITE_FUNCTION], STDOUT);
                close(fd[i][PIPE_WRITE_FUNCTION]);

            }

            run_command(node->pipe.parts[i]);
            exit(0);
            
        } else if (current_process > 0) { // Parent Process

            if (i > 0) {
                close(fd[i - 1][PIPE_READ_FUNCTION]);
                close(fd[i - 1][PIPE_WRITE_FUNCTION]);
            }

        } else if (current_process == -1) { // Error

            perror(NULL);
            exit(0);

        }
    }

    // Close all the pipe file descriptors
    for (int i = 0; i < number_pipe_parts - 1; i++) {
        close(fd[i][PIPE_READ_FUNCTION]);
        close(fd[i][PIPE_WRITE_FUNCTION]);
    }

    waitpid(current_process, &status, 0); // We only wait for the last process and not all to achieve the synchronous behavior
}



void run_command(node_t *node) {

    if (prompt) {
        prompt = "shellLine$";
    }

    //print_tree(node);

    switch (node->type) {
        case NODE_COMMAND: {
            
            signal(SIGINT, signal_handler);
            
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
        case NODE_PIPE: {
           
            run_pipe(node);

            break;
        }
        case NODE_REDIRECT:
            break;
        case NODE_SEQUENCE:{
            // This recursively breaks down the sequence commands until they are of type NODE_COMMAND
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
