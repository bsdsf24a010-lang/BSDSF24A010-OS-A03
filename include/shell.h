#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#define MAX_LEN 512
#define MAXARGS 10
#define ARGLEN 30
#define PROMPT "FCIT> "

// Function prototypes
char* read_cmd(char* prompt, FILE* fp);
char** tokenize(char* cmdline);
int execute(char** arglist);

int handle_builtin(char** arglist);

#define HISTORY_SIZE 20
extern char* history[HISTORY_SIZE];
extern int history_count;
void add_to_history(const char* cmd);
void print_history(void);

#include <readline/readline.h>
#include <readline/history.h>

void reap_zombies(void);
void print_jobs(void);
int execute_bg(char* arglist[], int is_bg, const char* raw_cmd);

#endif // SHELL_H
