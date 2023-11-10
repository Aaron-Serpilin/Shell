#include "parser/ast.h"
#include "shell.h"
#include <stdio.h>  
#include <stdlib.h> 
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <pwd.h>

#define PIPE_READ_FUNCTION 0
#define PIPE_WRITE_FUNCTION 1
#define STDIN 0
#define STDOUT 1

// Function to replace the strings for the prompt found in Stack Overflow
char *replace_string (char *original_message, char *string_to_replace, char *replacement) {

    char *result, *ins, *tmp;     
    int len_rep, len_with, len_front, count;   

    len_rep = strlen(string_to_replace);
    len_with = strlen(replacement);
    ins = original_message;

    for (count = 0; (tmp = strstr(ins, string_to_replace)); ++count) {
        ins = tmp + len_rep;
    }

    tmp = result = malloc(strlen(string_to_replace) + (len_with - len_rep) * count + 1);

    while (count--) {
        ins = strstr(original_message, string_to_replace);
        len_front = ins - original_message;
        tmp = strncpy(tmp, original_message, len_front) + len_front;
        tmp = strcpy(tmp, replacement) + len_with;
        original_message += len_front + len_rep;
    }

    strcpy(tmp, original_message);
    return result;
}

char *update_prompt_line (char *shell_line, struct passwd *username_string, char *host_name, char *current_working_directory) {

    if (shell_line) {
        shell_line = replace_string(shell_line, "\\u", username_string->pw_name);
        shell_line = replace_string(shell_line, "\\w", current_working_directory);
        shell_line = replace_string(shell_line, "\\h", host_name);
    }

    return shell_line;
}

void signal_handler (int signum) {
    printf("Interrupt Triggered. Terminating terminal with signal %d\n", signum);
}

void initialize(void) {

    signal(SIGINT, signal_handler); // Handles Interrupts
   
    uid_t username_code = getuid();
    struct passwd *username_string = getpwuid(username_code);
    char cwd[1024]; 
    getcwd(cwd, sizeof(cwd));
    char hostname[1024];
    gethostname(hostname, sizeof(hostname));
    char *shell_line = getenv("PS1"); // The prompt is always initialized randomly under PS1

    shell_line = update_prompt_line(shell_line, username_string, hostname, cwd);

    if (prompt) {
        printf("%s", shell_line);
        prompt = "$";
    }
    
}
   
void execute_command (char* command, char** arguments, int status, bool isDetached) {

    pid_t childProcess = fork();

    if (childProcess == 0) { // Means the current process is a child
        execvp(command, arguments);
        perror(NULL); // Acts like a catch statement and returns the corresponding error if the execution fails
    } else if (childProcess > 0 && !isDetached) { // This if for parent process that wait the child to return some sort of data, and then kill the process
        waitpid(childProcess, &status, 0);
    } 
    
}

void execute_actionable_commands (node_t *node, bool isDetached) {

            char *shellCommand = node->command.program;
            char **argv = node->command.argv;
            int status = 0;
            
            if (strcmp(shellCommand, "exit") == 0) { // Actionable commands are done in the parent and do not require forking protection

                if (node->command.argc > 1) {
                    exit(atoi(argv[1]));
                }

            } else if (strcmp(shellCommand, "cd") == 0) {
               
                chdir(argv[1]);

            } else if (strcmp(shellCommand, "set") == 0) {
        
                putenv(argv[1]);

            } else if (strcmp(shellCommand, "unset") == 0) {

                unsetenv(argv[1]);

            } else { // This applies to any command that returns value. We make a child to not overwrite the parent

                execute_command(shellCommand, argv, status, isDetached);

            }
}

void run_pipe(node_t *node) {

    int number_pipe_parts = node->pipe.n_parts;
    pid_t current_process;
    int fd[number_pipe_parts - 1][2]; // Array of pipes

    for (int i = 0; i < number_pipe_parts; i++) {

        pipe(fd[i]);

        current_process = fork(); // Each process is carried out in its own fork

        if (current_process == 0) { // Child Process
           
           if (i > 0) { 

                dup2(fd[i - 1][PIPE_READ_FUNCTION], STDIN);
                close(fd[i - 1][PIPE_READ_FUNCTION]);
                close(fd[i -1][PIPE_WRITE_FUNCTION]);

            } 
            
            if (i < number_pipe_parts - 1) { 

                dup2(fd[i][PIPE_WRITE_FUNCTION], STDOUT);
                close(fd[i][PIPE_WRITE_FUNCTION]);
                close(fd[i][PIPE_READ_FUNCTION]);

            }

            run_command(node->pipe.parts[i]);
            exit(0);
            
        } else if (current_process < 0) {

            perror(NULL);
            exit(0);

        } else {

            if (i > 0) {
                close(fd[i - 1][PIPE_READ_FUNCTION]);
                close(fd[i - 1][PIPE_WRITE_FUNCTION]);
            }

        } 
        
    }
       
    for (int i = 0; i < number_pipe_parts - 1; i++) {
        close(fd[i][PIPE_WRITE_FUNCTION]);
        close(fd[i][PIPE_READ_FUNCTION]);
    }

    waitpid(current_process, NULL, 0); // We only wait for the last process and not all to achieve the synchronous behavior
}

void run_detach (node_t *node) {

    pid_t childProcess = fork();
    if (childProcess == 0) {
        setpgid(0, 0);
        node_t *detach_node = node->detach.child;
        run_command(detach_node);
        exit(1);
    } else if (childProcess < 0) {
        perror(NULL);
        exit(0);
    }

}

void run_subshell (node_t *node) {

    node_t *subshell_node = node->subshell.child;
    pid_t childProcess = fork();
    
    if (childProcess == 0) {
        run_command(subshell_node);
        exit(1);
    } else if (childProcess > 0) {
        waitpid(childProcess, NULL, 0);
    } else {
        perror(NULL);
        exit(0);
    }

}

void run_command(node_t *node) {

    if (prompt) {
        prompt = "shellLine$";
    }

    //print_tree(node);

    switch (node->type) {
        case NODE_COMMAND: {
            execute_actionable_commands(node, false);
            break;
        }

        case NODE_DETACH: {
            run_detach(node);
            break;
        }
            
        case NODE_PIPE: {
            run_pipe(node);
            break;
        }

        case NODE_REDIRECT:
            break;

        case NODE_SEQUENCE:{  // This recursively breaks down the sequence commands until they are of type NODE_COMMAND
            run_command(node->sequence.first);
            run_command(node->sequence.second);
            break;
        }
  
        case NODE_SUBSHELL:{   
            run_subshell(node);
            break;
        }
              
        default:
            break;
    }
}

