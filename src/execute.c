#include "shell.h"
#include <fcntl.h>

static void setup_redirection_and_exec(char** cmd_args) {
    char* exec_args[MAXARGS + 1];
    int j = 0;

    for (int i = 0; cmd_args[i] != NULL; i++) {
        if (strcmp(cmd_args[i], "<") == 0) {
            if (cmd_args[i + 1] == NULL) {
                fprintf(stderr, "Syntax error: expected input file after '<'\n");
                exit(1);
            }
            int fd = open(cmd_args[i + 1], O_RDONLY);
            if (fd < 0) {
                perror(cmd_args[i + 1]);
                exit(1);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
            i++;
        } else if (strcmp(cmd_args[i], ">") == 0) {
            if (cmd_args[i + 1] == NULL) {
                fprintf(stderr, "Syntax error: expected output file after '>'\n");
                exit(1);
            }
            int fd = open(cmd_args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                perror(cmd_args[i + 1]);
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
            i++;
        } else {
            exec_args[j++] = cmd_args[i];
        }
    }
    exec_args[j] = NULL;

    if (j == 0) exit(0);

    execvp(exec_args[0], exec_args);
    perror("Command not found");
    exit(1);
}

int execute(char* arglist[]) {
    char* work[MAXARGS + 1];
    char** cmds[MAXARGS];
    int num_cmds = 0;
    int k = 0;

    cmds[num_cmds++] = &work[0];
    for (int i = 0; arglist[i] != NULL; i++) {
        if (strcmp(arglist[i], "|") == 0) {
            work[k++] = NULL;
            if (arglist[i + 1] != NULL) {
                cmds[num_cmds++] = &work[k];
            }
        } else {
            work[k++] = arglist[i];
        }
    }
    work[k] = NULL;

    int prev_fd = -1;
    int pipefd[2];
    pid_t pids[MAXARGS];

    for (int i = 0; i < num_cmds; i++) {
        if (i < num_cmds - 1) {
            if (pipe(pipefd) < 0) {
                perror("pipe");
                return -1;
            }
        }

        pid_t cpid = fork();
        if (cpid < 0) {
            perror("fork failed");
            exit(1);
        } else if (cpid == 0) {
            if (prev_fd != -1) {
                dup2(prev_fd, STDIN_FILENO);
                close(prev_fd);
            }
            if (i < num_cmds - 1) {
                close(pipefd[0]);
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);
            }
            setup_redirection_and_exec(cmds[i]);
        } else {
            pids[i] = cpid;
            if (prev_fd != -1) {
                close(prev_fd);
            }
            if (i < num_cmds - 1) {
                close(pipefd[1]);
                prev_fd = pipefd[0];
            }
        }
    }

    int status;
    for (int i = 0; i < num_cmds; i++) {
        waitpid(pids[i], &status, 0);
    }
    return 0;
}

int handle_builtin(char** arglist) {
    if (strcmp(arglist[0], "exit") == 0) {
        printf("Exiting shell...\n");
        exit(0);
    }
    else if (strcmp(arglist[0], "cd") == 0) {
        if (arglist[1] == NULL) {
            fprintf(stderr, "cd: missing directory argument\n");
        } else if (chdir(arglist[1]) != 0) {
            perror("cd");
        }
        return 1;
    }
    else if (strcmp(arglist[0], "help") == 0) {
        printf("Built-in commands:\n");
        printf("  cd <directory>  - change the current directory\n");
        printf("  exit            - exit the shell\n");
        printf("  help            - show this help message\n");
        printf("  jobs            - list background jobs (not yet implemented)\n");
        return 1;
    }
    else if (strcmp(arglist[0], "jobs") == 0) {
        printf("Job control not yet implemented.\n");
        return 1;
    }
    else if (strcmp(arglist[0], "history") == 0) {
        print_history();
        return 1;
    }
    return 0;
}
