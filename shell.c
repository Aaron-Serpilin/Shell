#include "parser/ast.h"
#include "shell.h"
#include <stdio.h>  
#include <stdlib.h> 
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

#define PIPE_READ_FUNCTION 0
#define PIPE_WRITE_FUNCTION 1
#define STDIN 0
#define STDOUT 1

void initialize(void) {

    if (prompt) {
        prompt = "shellLine$ ";
    }
        
}

void execute_command (pid_t process, char* command, char** arguments, int status) {

    if (process == 0) { // Means the current process is a child
        execvp(command, arguments);
        perror(NULL); // Acts like a catch statement and returns the corresponding error if the execution fails
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
        case NODE_PIPE: {

            int fd[2];
            int fd_input = STDIN; // We use this variable to make sure the input of the next command is the output of the current command
            int number_pipe_parts = node->pipe.n_parts;

            if (pipe(fd) == -1) {
                perror(NULL);
            }

            for (int i = 0; i < number_pipe_parts; i++) {

                char *current_command = node->pipe.parts[i]->command.program;
                char **argv = node->pipe.parts[i]->command.argv;
                int status = 0;
                pid_t current_process = fork();

                if (current_process == 0) { // Child Process

                    if (i == 0) { // The first processes do not read from the pipe and only write onto it for the next process

                        close(fd[PIPE_READ_FUNCTION]); // Since we do not read, we close the pipe reading function
                        dup2(fd[PIPE_WRITE_FUNCTION], STDOUT); // We direct the standard output onto the pipe
                        close(fd[PIPE_WRITE_FUNCTION]); // Following the directing of the STDOUT we close the pipe writing function
                        execute_command(current_process, current_command, argv, status); // Since we directed the output onto the pipe, we can now perform operations and its output will be placed onto the pipe
                        exit(0); // We exit each time to signal the process is done and that the next process can start/the parent can retake control

                    } else if (i == number_pipe_parts - 1) { // The last process does not write anything onto the pipe since there is no process after it, and only reads from it since it has all the information it needs

                        close(fd[PIPE_WRITE_FUNCTION]); // Since we do not write onto the pipe, we close the pipe writing function
                        dup2(fd_input, STDIN); // We direct the standard input which was the output of the last process as the input of the last process
                        close(fd_input); // We close the the file descriptor to ensure no unnecessary file descriptors are open
                        execute_command(current_process, current_command, argv, status); // After taking the input of the last process, we execute the corresponding command
                        exit(0);

                    } else { // Middle processes

                        close(fd_input); // Close the standard input, it's not needed since we read from the previous process in the pipe
                        dup2(fd[PIPE_READ_FUNCTION], STDIN); // Enables the input of middle processes to read the output of previous processes
                        close(fd[PIPE_READ_FUNCTION]); // After directing the output of previous processes as the STDIN we no longer need to read data
                        dup2(fd[PIPE_WRITE_FUNCTION], STDOUT); // Redirects any standard output of the process to the write end of the pipe for the next process
                        close(fd[PIPE_WRITE_FUNCTION]); // After the redirection, any STDOUT will be added to the write end of the pipe, so the connection is already established
                        execute_command(current_process, current_command, argv, status);
                        exit(0);

                    }

                } else if (current_process > 0) { // Parent Process

                    close(fd[PIPE_WRITE_FUNCTION]);
                    if (fd_input != STDIN) {
                        close(fd_input);
                    }
                    fd_input = fd[PIPE_READ_FUNCTION];

                } else if (current_process == -1) { // Error

                    perror(NULL);
                    exit(0);

                }

                waitpid(current_process, &status, 0);
            }

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
